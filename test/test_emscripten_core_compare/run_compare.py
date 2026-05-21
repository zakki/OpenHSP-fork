#!/usr/bin/env python3
import argparse
import os
import re
import shutil
import subprocess
import sys
import uuid
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CASE_ROOT = Path(__file__).resolve().parent / "cases"
DEFAULT_PASS_DIR = CASE_ROOT / "pass"
DEFAULT_XFAIL_DIR = CASE_ROOT / "xfail"


@dataclass
class CommandResult:
    argv: list[str]
    returncode: int
    stdout: str
    stderr: str
    timed_out: bool = False


def normalize_output(data: str) -> str:
    data = data.replace("\r\n", "\n").replace("\r", "\n")
    ignored = {"gpiod initalize failed."}
    lines = []
    for line in data.splitlines(keepends=True):
        if line.rstrip("\n") in ignored:
            continue
        line = re.sub(r"^#Error (\d+) in line \d+ \([^)]+\)$", r"#Error \1 in line <line> (<file>)", line.rstrip("\n"))
        lines.append(line + "\n")
    return "".join(lines)


def expected_error(source: Path) -> int | None:
    for line in source.read_text(encoding="utf-8").splitlines()[:8]:
        match = re.search(r"expect-error\s+(\d+)", line)
        if match:
            return int(match.group(1))
    return None


def resolve_command(path: str, *, required: bool = True) -> str | None:
    if not path:
        if required:
            raise SystemExit("missing required command path")
        return None
    if os.sep in path or (os.altsep and os.altsep in path):
        resolved = Path(path)
        if not resolved.is_absolute():
            resolved = (Path.cwd() / resolved).resolve()
        if not resolved.exists():
            raise SystemExit(f"command not found: {path}")
        return str(resolved)
    found = shutil.which(path)
    if found is None:
        raise SystemExit(f"command not found in PATH: {path}")
    return found


def run_command(argv: list[str], cwd: Path, timeout: float) -> CommandResult:
    try:
        result = subprocess.run(
            argv,
            cwd=str(cwd),
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
        )
        return CommandResult(argv, result.returncode, result.stdout, result.stderr)
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout or ""
        stderr = exc.stderr or ""
        if isinstance(stdout, bytes):
            stdout = stdout.decode(errors="replace")
        if isinstance(stderr, bytes):
            stderr = stderr.decode(errors="replace")
        return CommandResult(argv, 124, stdout, stderr, timed_out=True)


def compile_case(hspcmp: str, source: Path, ax_path: Path, common_path: Path, timeout: float) -> None:
    cmd = [
        hspcmp,
        "-d",
        "-i",
        "-u",
        f"--compath={common_path}",
        f"-o{ax_path}",
        str(source),
    ]
    result = run_command(cmd, ROOT, timeout)
    if result.returncode != 0:
        raise RuntimeError(
            f"compile failed for {source.name}\n"
            f"command: {' '.join(cmd)}\n"
            f"stdout:\n{result.stdout}\n"
            f"stderr:\n{result.stderr}"
        )


def run_native(hsp3cl: str, ax_path: Path, timeout: float) -> CommandResult:
    return run_command([hsp3cl, str(ax_path)], ROOT, timeout)


def run_emscripten(node: str, hsp3cl_em: str, ax_arg: str, timeout: float) -> CommandResult:
    return run_command([node, hsp3cl_em, ax_arg], ROOT, timeout)


def describe_failure(result: CommandResult, label: str, source_name: str) -> None:
    if result.timed_out:
        print(f"{label} {source_name}: command timed out", file=sys.stderr)
    else:
        print(f"{label} {source_name}: exited {result.returncode}", file=sys.stderr)
    if result.stderr:
        print(result.stderr, file=sys.stderr)


def results_match(native: CommandResult, emscripten: CommandResult) -> bool:
    if native.returncode != emscripten.returncode:
        return False
    return normalize_output(native.stdout) == normalize_output(emscripten.stdout)


def compare_case(args: argparse.Namespace, source: Path, *, expect_match: bool) -> bool:
    tmp_root = ROOT / ".tmp" / "emscripten_core_compare" / uuid.uuid4().hex
    tmp_root.mkdir(parents=True, exist_ok=True)
    try:
        ax_path = tmp_root / f"{source.stem}.ax"
        ax_arg = ax_path.relative_to(ROOT).as_posix()
        compile_case(args.native_hspcmp, source, ax_path, args.common_path, args.timeout)

        native = run_native(args.native_hsp3cl, ax_path, args.timeout)
        emscripten = run_emscripten(args.node, args.emscripten_hsp3cl, ax_arg, args.timeout)
        matched = results_match(native, emscripten)
        error_code = expected_error(source)

        if expect_match:
            ok = True
            if error_code is None:
                if native.returncode != 0:
                    describe_failure(native, "FAIL native", source.name)
                    ok = False
                if emscripten.returncode != 0:
                    describe_failure(emscripten, "FAIL emscripten", source.name)
                    ok = False
            else:
                expected_marker = f"#Error {error_code}"
                native_stdout = normalize_output(native.stdout)
                emscripten_stdout = normalize_output(emscripten.stdout)
                if native.returncode == 0:
                    print(f"FAIL native {source.name}: expected error {error_code}", file=sys.stderr)
                    ok = False
                if emscripten.returncode == 0:
                    print(f"FAIL emscripten {source.name}: expected error {error_code}", file=sys.stderr)
                    ok = False
                if expected_marker not in native_stdout:
                    print(f"FAIL native {source.name}: missing {expected_marker}", file=sys.stderr)
                    ok = False
                if expected_marker not in emscripten_stdout:
                    print(f"FAIL emscripten {source.name}: missing {expected_marker}", file=sys.stderr)
                    ok = False
            if not matched:
                print(f"FAIL {source.name}: stdout differs", file=sys.stderr)
                print("--- native stdout ---", file=sys.stderr)
                print(normalize_output(native.stdout), file=sys.stderr)
                print("--- emscripten stdout ---", file=sys.stderr)
                print(normalize_output(emscripten.stdout), file=sys.stderr)
                ok = False
            if ok:
                print(f"PASS {source.name}")
            return ok

        if matched:
            print(f"XPASS {source.name}: known issue no longer differs", file=sys.stderr)
            return False

        print(f"XFAIL {source.name}")
        return True
    finally:
        shutil.rmtree(tmp_root, ignore_errors=True)


def discover_cases(paths: list[str], mode: str) -> list[tuple[Path, bool]]:
    if paths:
        return [(Path(p).resolve(), mode != "xfail") for p in paths]

    cases: list[tuple[Path, bool]] = []
    if mode in {"pass", "all"}:
        cases.extend((path, True) for path in sorted(DEFAULT_PASS_DIR.glob("*.hsp")))
    if mode in {"xfail", "all"}:
        cases.extend((path, False) for path in sorted(DEFAULT_XFAIL_DIR.glob("*.hsp")))
    return cases


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare native hsp3cl output with Emscripten hsp3cl running on Node.js."
    )
    parser.add_argument("cases", nargs="*", help="case .hsp files; defaults depend on --mode")
    parser.add_argument("--mode", choices=("pass", "xfail", "all"), default="pass")
    parser.add_argument("--native-hspcmp", default=str(ROOT / "hspcmp"))
    parser.add_argument("--native-hsp3cl", default=str(ROOT / "hsp3cl"))
    parser.add_argument("--emscripten-hsp3cl", default=os.environ.get("HSP3CL_EM", ""))
    parser.add_argument("--node", default=os.environ.get("NODE", "node"))
    parser.add_argument("--common-path", type=Path, default=ROOT / "common")
    parser.add_argument("--timeout", type=float, default=5.0)
    args = parser.parse_args()

    args.native_hspcmp = resolve_command(args.native_hspcmp)
    args.native_hsp3cl = resolve_command(args.native_hsp3cl)
    args.node = resolve_command(args.node)
    if not args.emscripten_hsp3cl:
        raise SystemExit("set HSP3CL_EM or pass --emscripten-hsp3cl with the Node.js Emscripten hsp3cl file")
    args.emscripten_hsp3cl = resolve_command(args.emscripten_hsp3cl)

    cases = discover_cases(args.cases, args.mode)
    if not cases:
        raise SystemExit("no test cases found")
    missing = [str(case) for case, _ in cases if not case.exists()]
    if missing:
        raise SystemExit("missing test case(s): " + ", ".join(missing))

    all_ok = True
    for case, expect_match in cases:
        all_ok = compare_case(args, case, expect_match=expect_match) and all_ok
    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
