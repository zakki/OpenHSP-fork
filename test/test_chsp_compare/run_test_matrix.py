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

ROOT = Path(__file__).resolve().parents[2]
TEST_DIR = Path(__file__).resolve().parent
TEMPLATE_DIR = TEST_DIR / "templates"

HSP_CASES = [
    "operators_hsp",
    "features_hsp",
    "commands_hsp",
    "branches_hsp",
    "nested_hsp",
    "vectors_hsp",
    "scalars_hsp",
    "trig_hsp",
    "random_hsp",
    "inline_if_hsp",
    "inline_else_hsp",
    "inline_else_if_hsp",
    "inline_else_block_hsp",
    "mixed_if_hsp",
    "loop_return_hsp",
    "loop_return_value_hsp",
    "precedence_hsp",
    "separators_hsp",
    "continuations_hsp",
    "int_literals_hsp",
    "array_access_hsp",
    "local_arrays_hsp",
    "array_return_hsp",
    "array_builtin_args_hsp",
    "array_control_hsp",
    "case_fold_hsp",
    "double_return_hsp",
]

CHSP_CASES = [
    "operators_chsp",
    "features_chsp",
    "commands_chsp",
    "branches_chsp",
    "nested_chsp",
    "vectors_chsp",
    "scalars_chsp",
    "trig_chsp",
    "random_chsp",
    "inline_if_chsp",
    "inline_else_chsp",
    "inline_else_if_chsp",
    "inline_else_block_chsp",
    "mixed_if_chsp",
    "loop_return_chsp",
    "loop_return_value_chsp",
    "precedence_chsp",
    "separators_chsp",
    "continuations_chsp",
    "int_literals_chsp",
    "array_access_chsp",
    "local_arrays_chsp",
    "array_return_chsp",
    "array_builtin_args_chsp",
    "array_control_chsp",
    "case_fold_chsp",
    "double_return_chsp",
]

CASE_PAIRS = list(zip(HSP_CASES, CHSP_CASES))
IGNORED_OUTPUT_LINE = "gpiod initalize failed."
EXPECTED_COMPARE_MISMATCHES = {
    "c": {"double_return_chsp"},
    "default": set(),
    "libtcc": set(),
}
SECTION_HEADER_PATTERN = re.compile(r"^@@\s+([a-z0-9_]+)\s*$")
SECTION_REF_PATTERN = re.compile(r"\{\{([a-z0-9_]+)\}\}")
CASE_TEMPLATES = {
    "trig": {
        "path": TEMPLATE_DIR / "trig.case",
        "hsp_case": "trig_hsp",
        "chsp_case": "trig_chsp",
    },
}


def env_path(name: str, default: Path) -> Path:
    return Path(os.environ.get(name, str(default))).resolve()


def env_text(name: str, default: str) -> str:
    return os.environ.get(name, default)


HSPCMP = env_path("HSPCMP", ROOT / "hspcmp")
HSP3CL = env_path("HSP3CL", ROOT / "hsp3cl")
COMPATH = env_path("COMPATH", ROOT / "common")
C_TARGET_DIR = Path(env_text("C_TARGET_DIR", "c_target"))
C_LIBTCC_TARGET_DIR = Path(env_text("C_LIBTCC_TARGET_DIR", "c_target_libtcc"))
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
C_LIBTCC_HSPCMP_FLAGS = [
    "-d",
    "-i",
    "-u",
    "--chsp-compile=libtcc",
    f"--compath={with_trailing_sep(COMPATH)}",
]


class CommandError(RuntimeError):
    pass


def run(
    args: list[str],
    *,
    cwd: Path = TEST_DIR,
    env: dict[str, str] | None = None,
    expected: int = 0,
    allow_nonzero: bool = False,
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
    if allow_nonzero:
        if proc.returncode == 0:
            raise CommandError(f"command unexpectedly succeeded: {shlex.join(args)}")
        return proc.stdout if capture else ""
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


def copy_case_source(case: str, suffix: str, target_dir: Path) -> Path:
    ensure_generated_case_sources(case)
    src = TEST_DIR / f"{case}{suffix}"
    dst = target_dir / src.name
    ensure_parent(dst)
    shutil.copy2(src, dst)
    return dst


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


def parse_case_template(path: Path) -> dict[str, str]:
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
                raise CommandError(f"template content before first section: {path}")
            continue
        current_lines.append(raw_line)
    if current_name is not None:
        sections[current_name] = current_lines
    if not sections:
        raise CommandError(f"template has no sections: {path}")
    return {name: "".join(lines) for name, lines in sections.items()}


def render_template_sections(sections: dict[str, str], name: str, stack: tuple[str, ...] = ()) -> str:
    if name not in sections:
        raise CommandError(f"missing template section: {name}")
    if name in stack:
        chain = " -> ".join((*stack, name))
        raise CommandError(f"cyclic template section reference: {chain}")

    def replace(match: re.Match[str]) -> str:
        section_name = match.group(1)
        return render_template_sections(sections, section_name, (*stack, name))

    return SECTION_REF_PATTERN.sub(replace, sections[name])


def generated_case_names() -> set[str]:
    names: set[str] = set()
    for entry in CASE_TEMPLATES.values():
        names.add(entry["hsp_case"])
        names.add(entry["chsp_case"])
    return names


def template_key_for_case(case: str) -> str | None:
    for key, entry in CASE_TEMPLATES.items():
        if case in {entry["hsp_case"], entry["chsp_case"]}:
            return key
    return None


def render_case_variants(template_key: str) -> dict[str, str]:
    template_info = CASE_TEMPLATES[template_key]
    sections = parse_case_template(template_info["path"])
    outputs = {
        "hsp": render_template_sections(sections, "hsp"),
        "chsp": render_template_sections(sections, "chsp"),
    }
    if "chsp_c" in sections:
        outputs["chsp_c"] = render_template_sections(sections, "chsp_c")
    else:
        outputs["chsp_c"] = rewrite_chsp_module_for_c_target(outputs["chsp"])
    return outputs


def ensure_generated_case_sources(case: str) -> None:
    template_key = template_key_for_case(case)
    if template_key is None:
        return
    template_info = CASE_TEMPLATES[template_key]
    rendered = render_case_variants(template_key)
    write_text(TEST_DIR / f'{template_info["hsp_case"]}.hsp', rendered["hsp"])
    write_text(TEST_DIR / f'{template_info["chsp_case"]}.chsp', rendered["chsp"])


def copy_c_target_chsp_case(case: str, target_dir: Path) -> Path:
    src = TEST_DIR / f"{case}.chsp"
    dst = target_dir / src.name
    ensure_parent(dst)
    template_key = template_key_for_case(case)
    if template_key is not None:
        rendered = render_case_variants(template_key)
        write_text(dst, rendered["chsp_c"])
        return dst
    text = src.read_text(encoding="utf-8")
    write_text(dst, rewrite_chsp_module_for_c_target(text))
    return dst


def write_text(path: Path, text: str) -> None:
    ensure_parent(path)
    path.write_text(text, encoding="utf-8")


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
    if mode == "c":
        return DEFAULT_TARGET_HSPCMP_FLAGS
    if mode == "libtcc":
        return C_LIBTCC_HSPCMP_FLAGS
    raise ValueError(f"unknown mode: {mode}")


def mode_dir(mode: str) -> Path | None:
    if mode == "default":
        return None
    if mode == "c":
        return TEST_DIR / C_TARGET_DIR
    if mode == "libtcc":
        return TEST_DIR / C_LIBTCC_TARGET_DIR
    raise ValueError(f"unknown mode: {mode}")


def case_path(case: str, suffix: str, mode: str) -> Path:
    directory = mode_dir(mode)
    base = TEST_DIR if directory is None else directory
    return base / f"{case}{suffix}"


def compile_with_hspcmp(
    source: Path,
    mode: str,
    transform_only: bool = False,
    quiet_success: bool = False,
) -> None:
    args = [str(HSPCMP), *hspcmp_flags(mode)]
    if transform_only:
        args.append("-t")
    args.append(source.name)
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


def run_ax(ax_path: Path, lib_dir: Path | None) -> str:
    env = shared_library_env(lib_dir)
    stdout = run([str(HSP3CL), ax_path.name], cwd=ax_path.parent, env=env, capture=True)
    return normalize_output(stdout)


def build_hsp_output(case: str) -> Path:
    ensure_generated_case_sources(case)
    source = TEST_DIR / f"{case}.hsp"
    compile_with_hspcmp(source, "default", quiet_success=True)
    out_path = TEST_DIR / f"{case}.out"
    write_text(out_path, run_ax(TEST_DIR / f"{case}.ax", None))
    return out_path


def build_chsp_output(case: str, mode: str) -> Path:
    ensure_generated_case_sources(case)
    directory = mode_dir(mode)
    if directory is None:
        source = TEST_DIR / f"{case}.chsp"
    else:
        directory.mkdir(parents=True, exist_ok=True)
        source = copy_c_target_chsp_case(case, directory)
    compile_with_hspcmp(source, mode, quiet_success=True)
    if mode == "c":
        compile_shared_library(source.with_suffix(".c"), source.with_suffix(".so"))
    out_path = source.with_suffix(".out")
    write_text(out_path, run_ax(source.with_suffix(".ax"), source.parent))
    return out_path


def command_compare(mode: str) -> None:
    expected_mismatches = EXPECTED_COMPARE_MISMATCHES[mode]
    for hsp_case, chsp_case in CASE_PAIRS:
        hsp_out = build_hsp_output(hsp_case)
        chsp_out = build_chsp_output(chsp_case, mode)
        matched = compare_text(hsp_out, chsp_out)
        if chsp_case in expected_mismatches:
            if matched:
                raise CommandError(f"unexpected match: {mode} {chsp_case}")
            print(f"Expected mismatch: {mode} {chsp_case}")
            continue
        if not matched:
            raise CommandError(f"compare failed: {mode} {chsp_case}")
        print(f"{chsp_case} pass")


def assert_exists(path: Path) -> None:
    if not path.exists():
        raise CommandError(f"missing expected file: {path}")


def assert_not_exists(path: Path) -> None:
    if path.exists():
        raise CommandError(f"unexpected file exists: {path}")


def assert_file_contains(path: Path, pattern: str) -> None:
    text = path.read_text(encoding="utf-8")
    if pattern not in text:
        raise CommandError(f"pattern not found in {path}: {pattern}")


def assert_file_matches(path: Path, pattern: str) -> None:
    text = path.read_text(encoding="utf-8")
    if re.search(pattern, text, flags=re.MULTILINE) is None:
        raise CommandError(f"regex not found in {path}: {pattern}")


def same_file_contents(left: Path, right: Path) -> bool:
    return left.read_bytes() == right.read_bytes()


def transform_outputs(case: str, mode: str) -> tuple[Path, Path, Path, Path]:
    ensure_generated_case_sources(case)
    source = case_path(case, ".chsp", mode)
    if mode != "default":
        source.parent.mkdir(parents=True, exist_ok=True)
        copy_c_target_chsp_case(case, source.parent)
    ax_path = source.with_suffix(".ax")
    chi_path = source.with_suffix(".chi")
    c_path = source.with_suffix(".c")
    transform_copy = source.with_suffix(".transform.c")
    for path in [ax_path, chi_path, c_path, transform_copy]:
        remove_if_exists(path)
    return source, ax_path, chi_path, c_path, transform_copy


def run_transform_case(case: str, mode: str) -> None:
    source, ax_path, chi_path, c_path, transform_copy = transform_outputs(case, mode)
    compile_with_hspcmp(source, mode, transform_only=True)
    assert_exists(chi_path)
    assert_exists(c_path)
    assert_not_exists(ax_path)
    assert_file_contains(chi_path, "#module m0")
    if mode == "default":
        assert_file_contains(chi_path, f'#regcmd "hsp3cmdinit", "{case}.so"')
    else:
        assert_file_matches(chi_path, r"#(deffunc|defcfunc|cfunc) ")
    shutil.copy2(c_path, transform_copy)
    compile_with_hspcmp(source, mode, transform_only=False)
    if not same_file_contents(c_path, transform_copy):
        raise CommandError(f"transformed C changed unexpectedly: {case} ({mode})")
    stamp = source.with_suffix(".transform-ok")
    stamp.touch()


def run_transform_cli_check_default() -> None:
    source = TEST_DIR / "operators_chsp.chsp"
    for suffix in [".ax", ".chi", ".c", ".transform.c"]:
        remove_if_exists(TEST_DIR / f"operators_chsp{suffix}")
    compile_with_hspcmp(source, "default", transform_only=True)
    assert_exists(TEST_DIR / "operators_chsp.chi")
    assert_exists(TEST_DIR / "operators_chsp.c")
    assert_not_exists(TEST_DIR / "operators_chsp.ax")

    hsp_source = TEST_DIR / "operators_hsp.hsp"
    for suffix in [".ax", ".chi", ".c", ".i"]:
        remove_if_exists(TEST_DIR / f"operators_hsp{suffix}")
    compile_with_hspcmp(hsp_source, "default", transform_only=True)
    assert_exists(TEST_DIR / "operators_hsp.ax")
    assert_not_exists(TEST_DIR / "operators_hsp.chi")
    assert_not_exists(TEST_DIR / "operators_hsp.c")

    args = [str(HSPCMP), "-p", "-t", *DEFAULT_TARGET_HSPCMP_FLAGS, "operators_chsp.chsp"]
    run(args, cwd=TEST_DIR, allow_nonzero=True)

    invalid = TEST_DIR / "invalid_cpp_name_chsp.chsp"
    for suffix in [".ax", ".hsp", ".c", ".i", ".chi", ".so"]:
        remove_if_exists(TEST_DIR / f"invalid_cpp_name_chsp{suffix}")
    compile_with_hspcmp(invalid, "default", transform_only=False)
    assert_exists(TEST_DIR / "invalid_cpp_name_chsp.c")
    assert_file_contains(TEST_DIR / "invalid_cpp_name_chsp.c", "chsp_func_class")


def run_transform_cli_check_c() -> None:
    target_dir = TEST_DIR / C_TARGET_DIR
    target_dir.mkdir(parents=True, exist_ok=True)
    copy_c_target_chsp_case("operators_chsp", target_dir)
    copy_case_source("operators_hsp", ".hsp", target_dir)
    copy_c_target_chsp_case("invalid_cpp_name_chsp", target_dir)

    for suffix in [".ax", ".chi", ".c"]:
        remove_if_exists(target_dir / f"operators_chsp{suffix}")
    compile_with_hspcmp(target_dir / "operators_chsp.chsp", "c", transform_only=True)
    assert_exists(target_dir / "operators_chsp.chi")
    assert_exists(target_dir / "operators_chsp.c")
    assert_not_exists(target_dir / "operators_chsp.ax")

    for suffix in [".ax", ".chi", ".c", ".i"]:
        remove_if_exists(target_dir / f"operators_hsp{suffix}")
    compile_with_hspcmp(target_dir / "operators_hsp.hsp", "c", transform_only=True)
    assert_exists(target_dir / "operators_hsp.ax")
    assert_not_exists(target_dir / "operators_hsp.chi")
    assert_not_exists(target_dir / "operators_hsp.c")

    args = [
        str(HSPCMP),
        "-p",
        "-t",
        f"--compath={with_trailing_sep(COMPATH)}",
        str((target_dir / "operators_chsp.chsp").name),
    ]
    run(args, cwd=target_dir, allow_nonzero=True)

    for suffix in [".ax", ".hsp", ".c", ".i", ".chi", ".so"]:
        remove_if_exists(target_dir / f"invalid_cpp_name_chsp{suffix}")
    compile_with_hspcmp(target_dir / "invalid_cpp_name_chsp.chsp", "c", transform_only=False)
    assert_exists(target_dir / "invalid_cpp_name_chsp.c")
    assert_file_contains(target_dir / "invalid_cpp_name_chsp.c", "chsp_func_class")


def command_transform(mode: str) -> None:
    for case in CHSP_CASES:
        run_transform_case(case, mode)
    if mode == "default":
        run_transform_cli_check_default()
    elif mode == "c":
        run_transform_cli_check_c()
    else:
        raise CommandError(f"transform is unsupported for mode: {mode}")


def command_clean() -> None:
    for case in HSP_CASES:
        for suffix in [".ax", ".i", ".chi", ".c", ".out"]:
            remove_if_exists(TEST_DIR / f"{case}{suffix}")
    for case in CHSP_CASES:
        for suffix in [".ax", ".hsp", ".c", ".i", ".so", ".chi", ".transform.c", ".out", ".transform-ok"]:
            remove_if_exists(TEST_DIR / f"{case}{suffix}")
    for name in ["invalid_cpp_name_chsp"]:
        for suffix in [".ax", ".hsp", ".c", ".i", ".chi", ".so"]:
            remove_if_exists(TEST_DIR / f"{name}{suffix}")
    remove_if_exists(TEST_DIR / C_TARGET_DIR)
    remove_if_exists(TEST_DIR / C_LIBTCC_TARGET_DIR)


for case in generated_case_names():
    ensure_generated_case_sources(case)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)

    compare = subparsers.add_parser("compare")
    compare.add_argument("--mode", choices=["default", "c", "libtcc"], required=True)

    transform = subparsers.add_parser("transform")
    transform.add_argument("--mode", choices=["default", "c"], required=True)

    subparsers.add_parser("clean")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "compare":
            command_compare(args.mode)
        elif args.command == "transform":
            command_transform(args.mode)
        elif args.command == "clean":
            command_clean()
        else:
            raise CommandError(f"unknown command: {args.command}")
    except CommandError as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
