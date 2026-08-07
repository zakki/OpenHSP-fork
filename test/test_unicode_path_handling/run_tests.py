#!/usr/bin/env python3
"""Unicode path smoke tests for the hspcmp command-line compiler."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import List, Optional


ROOT = Path(__file__).resolve().parents[2]


def discover_hspcmp() -> Optional[Path]:
    candidates = (
        ROOT / "src" / "hspcmp" / "Release64" / "hspcmp.exe",
        ROOT / "src" / "hspcmp" / "Release" / "hspcmp.exe",
        ROOT / "src" / "hspcmp" / "Release64" / "hspcmp",
        ROOT / "src" / "hspcmp" / "Release" / "hspcmp",
    )
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def run_command(command: List[str], cwd: Path) -> bytes:
    completed = subprocess.run(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if completed.returncode != 0:
        output = completed.stdout.decode("utf-8", errors="replace")
        raise RuntimeError(
            "command failed with exit code {}:\n  {}\n{}".format(
                completed.returncode, " ".join(map(str, command)), output
            )
        )
    return completed.stdout


def write_fixture(path: Path, text: str, encoding: str) -> None:
	with path.open("w", encoding=encoding, newline="") as fixture:
		fixture.write(text)


def run_compile(
    hspcmp: Path, common: Path, source: Path, output: Path, utf8_input: bool
) -> bytes:
    command = [
        str(hspcmp),
        "-d",
        "-o" + str(output),
        "--compath=" + str(common),
    ]
    if utf8_input:
        command.insert(2, "-i")
    return run_command(command + [str(source)], source.parent)


def test_utf8_source_and_include(hspcmp: Path, common: Path, workdir: Path) -> None:
    source_dir = workdir / "ソース-日本語"
    source_dir.mkdir()
    include = source_dir / "含む-日本語.as"
    source = source_dir / "主ソース.hsp"
    output = source_dir / "コンパイル結果.ax"

    write_fixture(include, 'mes "UTF8_INCLUDE_MARKER_日本語"\n', "utf-8")
    write_fixture(
        source,
        '#include "含む-日本語.as"\nmes "UTF8_MAIN_MARKER"\nmes __file__\n',
        "utf-8",
    )

    run_compile(hspcmp, common, source, output, utf8_input=True)
    result = output.read_bytes()
    assert b"UTF8_INCLUDE_MARKER_" in result
    assert source.name.encode("utf-8") in result


def test_cp932_source_and_include(hspcmp: Path, common: Path, workdir: Path) -> None:
    source_dir = workdir / "CP932-日本語"
    source_dir.mkdir()
    include = source_dir / "従来.as"
    source = source_dir / "CP932-main.hsp"
    output = source_dir / "cp932-output.ax"

    write_fixture(include, 'mes "CP932_INCLUDE_MARKER"\n', "cp932")
    write_fixture(
        source,
        '#include "従来.as"\nmes "CP932_MAIN_MARKER"\nmes __file__\n',
        "cp932",
    )

    run_compile(hspcmp, common, source, output, utf8_input=False)
    result = output.read_bytes()
    assert b"CP932_INCLUDE_MARKER" in result


def run_pathio(pathio_exe: Path) -> None:
    run_command([str(pathio_exe)], pathio_exe.parent)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hspcmp", type=Path, help="path to hspcmp executable")
    parser.add_argument(
        "--common", type=Path, default=ROOT / "common", help="hspcmp common directory"
    )
    parser.add_argument("--pathio-exe", type=Path, help="optional hsp3pathio test executable")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    hspcmp = args.hspcmp or discover_hspcmp()
    if hspcmp is None:
        print("hspcmp executable not found; pass --hspcmp PATH", file=sys.stderr)
        return 2
    hspcmp = hspcmp.resolve()
    common = args.common.resolve()
    if not hspcmp.is_file():
        print("hspcmp executable does not exist: {}".format(hspcmp), file=sys.stderr)
        return 2
    if not common.is_dir():
        print("common directory does not exist: {}".format(common), file=sys.stderr)
        return 2

    try:
        if args.pathio_exe:
            run_pathio(args.pathio_exe.resolve())
            print("PASS pathio executable")
        with tempfile.TemporaryDirectory(prefix="openhsp-unicode-path-") as temporary:
            workdir = Path(temporary)
            test_utf8_source_and_include(hspcmp, common, workdir)
            print("PASS UTF-8 source/include")
            test_cp932_source_and_include(hspcmp, common, workdir)
            print("PASS CP932 source/include")
    except (AssertionError, OSError, RuntimeError, UnicodeError) as error:
        print("FAIL: {}".format(error), file=sys.stderr)
        return 1
    print("Unicode path smoke tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
