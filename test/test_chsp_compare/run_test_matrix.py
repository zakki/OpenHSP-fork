#!/usr/bin/env python3

from __future__ import annotations

import argparse
import difflib
import os
import re
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

from generate_templates import (
    DEFAULT_OUTPUT_DIR,
    TOP_LEVEL_SECTIONS,
    discover_templates,
    generate_template,
    parse_template,
)

ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = Path(__file__).resolve().parent

IGNORED_OUTPUT_LINE = "gpiod initalize failed."
SECTION_HEADER_PATTERN = re.compile(r"^@@\s+([a-z0-9_]+)\s*$")
SECTION_REF_PATTERN = re.compile(r"\{\{([a-z0-9_]+)\}\}")


def env_path(name: str, default: Path) -> Path:
    return Path(os.environ.get(name, str(default))).resolve()


def env_text(name: str, default: str) -> str:
    return os.environ.get(name, default)


HSPCMP = env_path("HSPCMP", ROOT / "hspcmp")
HSP3CL = env_path("HSP3CL", ROOT / "hsp3cl")
COMPATH = env_path("COMPATH", ROOT / "common")
C_NATIVE_CC = env_text("C_NATIVE_CC", "cc")
C_NATIVE_CFLAGS = env_text("C_NATIVE_CFLAGS", f"-std=c11 -I{ROOT}")
C_NATIVE_SOFLAGS = env_text("C_NATIVE_SOFLAGS", "-shared -fPIC")
C_NATIVE_LDLIBS = env_text("C_NATIVE_LDLIBS", "-lm")


def with_trailing_sep(path: Path) -> str:
    text = str(path)
    if text.endswith(("/", "\\")):
        return text
    return text + os.sep


DEFAULT_TARGET_HSPCMP_FLAGS = [
    "-d",
    "-i",
    "-u",
    f"--compath={with_trailing_sep(COMPATH)}",
]
EMIT_C_HSPCMP_FLAGS = [
    "-d",
    "-i",
    "-u",
    "--chsp-compile=none",
    f"--compath={with_trailing_sep(COMPATH)}",
]
C_LIBTCC_HSPCMP_FLAGS = [
    "-d",
    "-i",
    "-u",
    "--chsp-compile=libtcc",
    f"--compath={with_trailing_sep(COMPATH)}",
]

TEMPLATE_CASES = tuple(
    sorted(
        path.stem
        for path in discover_templates([])
        if (TEST_DIR / f"{path.stem}.gt").exists()
    )
)
TEMPLATE_SPECS = {
    case: parse_template(TEST_DIR / f"{case}.template") for case in TEMPLATE_CASES
}

# mode selects the native compile path used by hspcmp for cHSP modules.
# variant selects which generated source kind from a template is executed.
MODE_HELP = (
    "mode selects the native compile path: "
    "default runs hspcmp as-is, emit-c writes .c and builds shared libraries "
    "with the external C compiler, libtcc lets hspcmp build shared libraries "
    "via libtcc."
)
VARIANT_HELP = (
    "variant selects which generated source kind from a template is executed: "
    "hsp is vanilla HSP, chsp_p is target=plugin, chsp_c is target=c."
)


class CommandError(RuntimeError):
    pass


def run(
    args: list[str],
    *,
    cwd: Path = TEST_DIR,
    env: dict[str, str] | None = None,
    expected: int = 0,
    capture: bool = False,
    quiet_success: bool = False,
) -> str:
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)
    capture_output = capture or quiet_success
    proc = subprocess.run(
        args,
        cwd=cwd,
        env=merged_env,
        text=True,
        capture_output=capture_output,
    )
    if proc.returncode != expected:
        output_parts = []
        if capture_output and proc.stdout:
            output_parts.append(proc.stdout)
        if capture_output and proc.stderr:
            output_parts.append(proc.stderr)
        detail = "".join(output_parts)
        raise CommandError(
            f"command failed ({proc.returncode}): {shlex.join(args)}\n{detail}".rstrip()
        )
    return proc.stdout if capture else ""


def normalize_output(text: str) -> str:
    lines = text.replace("\r", "").splitlines()
    filtered = [line for line in lines if line != IGNORED_OUTPUT_LINE]
    return "\n".join(filtered) + ("\n" if filtered else "")


def shared_library_env(lib_dir: Path | None) -> dict[str, str]:
    if lib_dir is None:
        return {}
    if sys.platform == "win32":
        key = "PATH"
    elif sys.platform == "darwin":
        key = "DYLD_LIBRARY_PATH"
    else:
        key = "LD_LIBRARY_PATH"
    current = os.environ.get(key, "")
    value = str(lib_dir.resolve())
    if current:
        value = value + os.pathsep + current
    return {key: value}


def ensure_parent(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)


def remove_if_exists(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists() or path.is_symlink():
        path.unlink()


def write_text(path: Path, text: str) -> None:
    ensure_parent(path)
    path.write_text(text, encoding="utf-8")


def rewrite_chsp_module_for_c_target(text: str) -> str:
    lines = text.splitlines(keepends=True)
    out_lines: list[str] = []
    pattern = re.compile(r"^(\s*#chsp_module\b)(.*?)(\r?\n?)$", flags=re.IGNORECASE)
    target_pattern = re.compile(r"\btarget\s*=\s*(plugin|c)\b", flags=re.IGNORECASE)
    for line in lines:
        match = pattern.match(line)
        if not match:
            out_lines.append(line)
            continue
        prefix, rest, newline = match.groups()
        if target_pattern.search(rest):
            rest = target_pattern.sub("target=c", rest)
        elif rest.strip():
            rest = f"{rest} target=c"
        else:
            rest = " target=c"
        out_lines.append(f"{prefix}{rest}{newline}")
    return "".join(out_lines)


def compare_text(expected_path: Path, actual_path: Path) -> bool:
    expected = expected_path.read_text(encoding="utf-8").splitlines(keepends=True)
    actual = actual_path.read_text(encoding="utf-8").splitlines(keepends=True)
    if expected == actual:
        return True
    diff = "".join(
        difflib.unified_diff(
            expected,
            actual,
            fromfile=str(expected_path),
            tofile=str(actual_path),
        )
    )
    sys.stderr.write(diff)
    return False


def hspcmp_flags(mode: str) -> list[str]:
    if mode == "default":
        return DEFAULT_TARGET_HSPCMP_FLAGS
    if mode == "emit-c":
        return EMIT_C_HSPCMP_FLAGS
    if mode == "libtcc":
        return C_LIBTCC_HSPCMP_FLAGS
    raise ValueError(f"unknown mode: {mode}")


def variants_for_mode(mode: str) -> tuple[str, ...]:
    # mode controls compilation policy; the returned variants control which
    # generated template outputs are compared for that policy.
    if mode == "default":
        return ("hsp", "chsp_p", "chsp_c")
    if mode in {"emit-c", "libtcc"}:
        return ("chsp_p", "chsp_c")
    raise ValueError(f"unknown mode: {mode}")


def compare_variants_for_case(case: str, mode: str) -> tuple[str, ...]:
    spec = TEMPLATE_SPECS[case]
    if mode in spec.compare_variants:
        return spec.compare_variants[mode]
    return tuple(
        variant
        for variant in variants_for_mode(mode)
        if variant in spec.top_level_sections and variant in TOP_LEVEL_SECTIONS
    )


def generated_source_path(case: str, variant: str, output_dir: Path) -> Path:
    if variant == "hsp":
        return output_dir / f"{case}.hsp"
    if variant == "chsp_c":
        return output_dir / f"{case}_chsp_c.hsp"
    if variant == "chsp_p":
        return output_dir / f"{case}_chsp_p.hsp"
    raise ValueError(f"unknown variant: {variant}")


def generate_case_sources(case: str, output_dir: Path) -> dict[str, Path]:
    template_path = TEST_DIR / f"{case}.template"
    generate_template(template_path, output_dir)
    return {
        variant: generated_source_path(case, variant, output_dir)
        for variant in TEMPLATE_SPECS[case].top_level_sections
    }


def compile_with_hspcmp(
    source: Path,
    mode: str,
    *,
    quiet_success: bool = False,
) -> None:
    args = [str(HSPCMP), *hspcmp_flags(mode), source.name]
    run(args, cwd=source.parent, quiet_success=quiet_success)


def compile_shared_library(c_path: Path, output_path: Path) -> None:
    rel_c_path = os.path.relpath(c_path, TEST_DIR)
    rel_output_path = os.path.relpath(output_path, TEST_DIR)
    args = [
        *shlex.split(C_NATIVE_CC),
        *shlex.split(C_NATIVE_CFLAGS),
        *shlex.split(C_NATIVE_SOFLAGS),
        rel_c_path,
        "-o",
        rel_output_path,
        *shlex.split(C_NATIVE_LDLIBS),
    ]
    run(args, cwd=TEST_DIR, quiet_success=True)


def compiled_artifact_paths(source: Path, variant: str) -> tuple[Path, Path]:
    return source.with_suffix(".c"), source.with_suffix(".so")


def run_ax(ax_path: Path, lib_dir: Path | None) -> str:
    env = shared_library_env(lib_dir)
    stdout = run([str(HSP3CL), ax_path.name], cwd=ax_path.parent, env=env, capture=True)
    return normalize_output(stdout)


def build_variant_output(source: Path, variant: str, mode: str) -> Path:
    compile_with_hspcmp(source, mode, quiet_success=True)
    if variant == "chsp_c" and mode == "emit-c":
        c_path, so_path = compiled_artifact_paths(source, variant)
        compile_shared_library(c_path, so_path)
        lib_dir: Path | None = source.parent
    else:
        lib_dir = None
    out_path = source.with_suffix(".out")
    write_text(out_path, run_ax(source.with_suffix(".ax"), lib_dir))
    return out_path


def command_compare(mode: str, output_dir: Path) -> None:
    for case in TEMPLATE_CASES:
        expected = TEST_DIR / f"{case}.gt"
        generated = generate_case_sources(case, output_dir)
        for variant in compare_variants_for_case(case, mode):
            actual = build_variant_output(generated[variant], variant, mode)
            if not compare_text(expected, actual):
                raise CommandError(f"compare failed: {mode} {case} ({variant})")
            print(f"{case} {variant} pass")


def command_clean(output_dir: Path) -> None:
    remove_if_exists(output_dir)
    for stem in TEMPLATE_CASES:
        for suffix in [".ax", ".chi", ".c", ".i", ".out", ".so"]:
            remove_if_exists(TEST_DIR / f"{stem}{suffix}")
            remove_if_exists(TEST_DIR / f"{stem}_chsp_c{suffix}")
            remove_if_exists(TEST_DIR / f"{stem}_chsp_p{suffix}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate template cases, run selected variants, and compare stdout to .gt files."
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=DEFAULT_OUTPUT_DIR,
        help=f"Directory for generated template sources (default: {DEFAULT_OUTPUT_DIR}).",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    compare = subparsers.add_parser(
        "compare",
        description=f"{MODE_HELP} {VARIANT_HELP}",
        help="Generate sources, run selected variants, and compare stdout to .gt files.",
    )
    compare.add_argument(
        "--mode",
        choices=["default", "emit-c", "libtcc"],
        required=True,
        help=MODE_HELP,
    )

    subparsers.add_parser("clean", help="Remove generated sources and compiled test artifacts.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output_dir = args.output_dir.resolve()
    try:
        if args.command == "compare":
            command_compare(args.mode, output_dir)
        elif args.command == "clean":
            command_clean(output_dir)
        else:
            raise CommandError(f"unknown command: {args.command}")
    except CommandError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
