#!/usr/bin/env python3
"""Compile real HSP fixtures and execute the Emscripten interpreter branch."""
import argparse
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
CASES = {
    "nested_returns": (0, ["67", "text=5", "5.500000"] * 3 + ["0", "0", "DONE waits=0"]),
    "command_loop": (0, ["6", "3", "0", "0", "DONE waits=7"]),
    "callbacks": (0, ["1", "nested", "gosub", "2", "nested", "gosub", "caller", "DONE waits=0"]),
    "calls": (0, ["1", "2", "caller", "DONE waits=2"]),
    "invalid_callback": (1, ["ERROR 42"]),
    "on_gosub": (0, ["before", "outer-before", "inner", "outer-after", "after", "DONE waits=1"]),
    "on_gosub_function": (0, ["10", "caller", "DONE waits=0"]),
    "on_gosub_callback": (0, ["before", "sub", "after"] * 2 + ["caller", "DONE waits=0"]),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hspcmp", type=Path, default=ROOT / "hspcmp")
    parser.add_argument("--runner", type=Path, default=HERE / "build/runner")
    parser.add_argument("--known-gaps", action="store_true",
                        help="run unresolved reproducers; exits nonzero until fixed")
    args = parser.parse_args()
    compiler, runner = args.hspcmp.resolve(), args.runner.resolve()
    failures = 0
    with tempfile.TemporaryDirectory(prefix="hsp-continuations-") as tmp:
        tmp = Path(tmp)
        (tmp / "test_api.as").write_bytes((HERE / "test_api.as").read_bytes())
        cases = CASES if not args.known_gaps else {
            "synchronous_callback": (0, ["callback"] * 2 + ["7", "caller", "DONE waits=0"]),
            "nested_callback": (0, ["outer"] * 2 + ["inner"] * 4 + ["caller", "DONE waits=0"]),
        }
        fixture_dir = HERE / "known_gaps" if args.known_gaps else HERE
        for name, (status, lines) in cases.items():
            source = tmp / (name + ".hsp")
            source.write_bytes((fixture_dir / source.name).read_bytes())
            compiled = subprocess.run(
                [str(compiler), "-i", "-u", "--compath=" + str(ROOT / "common") + "/", str(source)],
                cwd=tmp, capture_output=True, text=True, timeout=30)
            if compiled.returncode or not source.with_suffix(".ax").exists():
                raise RuntimeError(compiled.stdout + compiled.stderr)
            result = subprocess.run([str(runner), str(source.with_suffix(".ax"))],
                                    cwd=tmp, capture_output=True, text=True, timeout=10)
            actual = result.stdout.splitlines()
            # Gap reproducers require exactly-once completion, without prescribing
            # whether nested callbacks should run immediately or after their parent.
            if args.known_gaps:
                actual, lines = sorted(actual), sorted(lines)
            if result.returncode != status or actual != lines or result.stderr:
                failures += 1
                print(f"FAIL {name}: expected status={status}, lines={lines!r}")
                print(f"  actual status={result.returncode}, stdout={result.stdout!r}, stderr={result.stderr!r}")
            else:
                print(f"PASS {name}")
    return bool(failures)


if __name__ == "__main__":
    raise SystemExit(main())
