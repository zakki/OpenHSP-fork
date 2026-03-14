#!/usr/bin/env python3

from __future__ import annotations

import argparse
from dataclasses import dataclass
import re
import shutil
import sys
from pathlib import Path

TEST_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT_DIR = TEST_DIR / "gen"
SECTION_HEADER_PATTERN = re.compile(r"^@@\s+([a-z0-9_]+)\s*$")
SECTION_REF_PATTERN = re.compile(r"\{\{([a-z0-9_]+)\}\}")
TOP_LEVEL_SECTIONS = ("hsp", "chsp_c", "chsp_p")
COMPARE_VARIANT_KEYS = {
    "compare_default": "default",
    "compare_emit_c": "emit-c",
    "compare_libtcc": "libtcc",
}


class TemplateError(RuntimeError):
    pass


@dataclass(frozen=True)
class TemplateSpec:
    sections: dict[str, str]
    top_level_sections: tuple[str, ...]
    compare_variants: dict[str, tuple[str, ...]]


def parse_variant_list(raw_value: str, *, path: Path, key: str) -> tuple[str, ...]:
    variants = tuple(
        item.strip()
        for item in raw_value.replace(",", " ").split()
        if item.strip()
    )
    if not variants:
        raise TemplateError(f"empty variant list for {key} in {path}")
    unknown = [variant for variant in variants if variant not in TOP_LEVEL_SECTIONS]
    if unknown:
        raise TemplateError(
            f"unknown variants for {key} in {path}: {', '.join(sorted(set(unknown)))}"
        )
    return variants


def parse_meta_section(text: str, *, path: Path) -> dict[str, tuple[str, ...]]:
    compare_variants: dict[str, tuple[str, ...]] = {}
    for lineno, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise TemplateError(f"invalid meta line in {path}:{lineno}: {raw_line}")
        key, value = [part.strip() for part in line.split("=", 1)]
        if key not in COMPARE_VARIANT_KEYS:
            raise TemplateError(f"unknown meta key in {path}:{lineno}: {key}")
        compare_variants[COMPARE_VARIANT_KEYS[key]] = parse_variant_list(
            value, path=path, key=key
        )
    return compare_variants


def parse_template(path: Path) -> TemplateSpec:
    sections: dict[str, list[str]] = {}
    current_name: str | None = None
    current_lines: list[str] = []

    for raw_line in path.read_text(encoding="utf-8").splitlines(keepends=True):
        match = SECTION_HEADER_PATTERN.match(raw_line.rstrip("\r\n"))
        if match:
            if current_name is not None:
                sections[current_name] = current_lines
            current_name = match.group(1)
            current_lines = []
            continue
        if current_name is None:
            if raw_line.strip():
                raise TemplateError(f"template content before first section: {path}")
            continue
        current_lines.append(raw_line)

    if current_name is not None:
        sections[current_name] = current_lines
    if not sections:
        raise TemplateError(f"template has no sections: {path}")

    parsed = {name: "".join(lines) for name, lines in sections.items()}
    top_level_sections = tuple(name for name in TOP_LEVEL_SECTIONS if name in parsed)
    if not top_level_sections:
        raise TemplateError(f"missing top-level sections in {path}")

    compare_variants = {}
    if "meta" in parsed:
        compare_variants = parse_meta_section(parsed["meta"], path=path)
        for mode, variants in compare_variants.items():
            missing = [variant for variant in variants if variant not in top_level_sections]
            if missing:
                raise TemplateError(
                    f"compare variants for {mode} missing top-level sections in {path}: "
                    f"{', '.join(missing)}"
                )

    return TemplateSpec(
        sections=parsed,
        top_level_sections=top_level_sections,
        compare_variants=compare_variants,
    )


def render_section(
    sections: dict[str, str],
    name: str,
    *,
    template_path: Path,
    stack: tuple[str, ...] = (),
) -> str:
    if name not in sections:
        raise TemplateError(f"missing template section in {template_path}: {name}")
    if name in stack:
        chain = " -> ".join((*stack, name))
        raise TemplateError(f"cyclic template section reference in {template_path}: {chain}")

    def replace(match: re.Match[str]) -> str:
        section_name = match.group(1)
        return render_section(
            sections,
            section_name,
            template_path=template_path,
            stack=(*stack, name),
        )

    return SECTION_REF_PATTERN.sub(replace, sections[name])


def render_template(path: Path) -> dict[str, str]:
    spec = parse_template(path)
    sections = spec.sections

    def render_top_level(name: str) -> str:
        text = render_section(sections, name, template_path=path)
        return text[:-1] if text.endswith("\n") else text

    return {name: render_top_level(name) for name in spec.top_level_sections}


def output_paths(template_path: Path, output_dir: Path) -> dict[str, Path]:
    stem = template_path.stem
    return {
        "hsp": output_dir / f"{stem}.hsp",
        "chsp_c": output_dir / f"{stem}_chsp_c.hsp",
        "chsp_p": output_dir / f"{stem}_chsp_p.hsp",
    }


def generate_template(template_path: Path, output_dir: Path) -> list[Path]:
    rendered = render_template(template_path)
    outputs = output_paths(template_path, output_dir)
    written: list[Path] = []
    for name, text in rendered.items():
        path = outputs[name]
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        written.append(path)
    return written


def discover_templates(template_paths: list[Path]) -> list[Path]:
    if template_paths:
        return sorted(path.resolve() for path in template_paths)
    return sorted(TEST_DIR.glob("*.template"))


def clean_output_dir(output_dir: Path) -> None:
    if output_dir.exists():
        shutil.rmtree(output_dir)


def display_path(path: Path) -> Path:
    try:
        return path.relative_to(TEST_DIR)
    except ValueError:
        return path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Render test/test_chsp_compare/*.template into hsp variants."
    )
    parser.add_argument(
        "templates",
        nargs="*",
        type=Path,
        help="Template files to render. Defaults to all *.template in the test directory.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Directory for generated files (default: {DEFAULT_OUTPUT_DIR}).",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Remove the output directory and exit.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    output_dir = args.output_dir.resolve()

    if args.clean:
        clean_output_dir(output_dir)
        return 0

    templates = discover_templates(args.templates)
    if not templates:
        print("No template files found.", file=sys.stderr)
        return 0

    try:
        written_count = 0
        for template_path in templates:
            for path in generate_template(template_path, output_dir):
                print(display_path(path))
                written_count += 1
    except TemplateError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    if written_count == 0:
        print("No files generated.", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
