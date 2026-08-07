#!/usr/bin/env python3
"""Unicode path smoke tests for the hspcmp command-line compiler."""

from __future__ import annotations

import argparse
import ctypes
from contextlib import contextmanager
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Callable, List, Optional


ROOT = Path(__file__).resolve().parents[2]


def discover_hspcmp() -> Optional[Path]:
    candidates = (
        ROOT / "src" / "hspcmp" / "Release64" / "hspcmp.exe",
        ROOT / "src" / "hspcmp" / "Release" / "hspcmp.exe",
        ROOT / "src" / "hspcmp" / "Release64" / "hspcmp",
        ROOT / "src" / "hspcmp" / "Release" / "hspcmp",
    )
    return next((candidate for candidate in candidates if candidate.is_file()), None)


def discover_hspcmp_dll() -> Optional[Path]:
    candidates = (
        ROOT / "src" / "hspcmp" / "Release64" / "hspcmp_64.dll",
        ROOT / "src" / "hspcmp" / "Release" / "hspcmp.dll",
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


def require_marker(result: bytes, marker: bytes, description: str, output: Path) -> None:
    if marker not in result:
        raise RuntimeError(
            "{} missing from {} ({} bytes)".format(
                description, output, len(result)
            )
        )


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
    require_marker(result, b"UTF8_INCLUDE_MARKER_", "UTF-8 include marker", output)
    require_marker(result, source.name.encode("utf-8"), "UTF-8 source filename", output)


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


def encode_legacy_path(path: Path) -> bytes:
    if os.name != "nt":
        raise RuntimeError("DLL path tests require Windows")
    return str(path).encode("mbcs")


class HspcmpDll:
    def __init__(self, path: Path) -> None:
        if os.name != "nt":
            raise RuntimeError("DLL path tests require Windows")

        self._dll_directory = None
        add_dll_directory = getattr(os, "add_dll_directory", None)
        if add_dll_directory is not None:
            self._dll_directory = add_dll_directory(str(path.parent))
        try:
            self._dll = ctypes.WinDLL(str(path))
        except OSError:
            if self._dll_directory is not None:
                self._dll_directory.close()
                self._dll_directory = None
            raise

        self._hsp_ptr_int = ctypes.c_ssize_t
        self.hsc_ini = self._path_function("hsc_ini")
        self.hsc_compath = self._path_function("hsc_compath")
        self.hsc_objname = self._path_function("hsc_objname")
        self.hsc_comp = self._int_function(
            "hsc_comp",
            [self._hsp_ptr_int] * 4,
        )
        self.hsc_getmes = self._int_function(
            "hsc_getmes",
            [ctypes.c_char_p] + [self._hsp_ptr_int] * 3,
        )
        self.hsc_bye = self._int_function(
            "hsc_bye",
            [self._hsp_ptr_int] * 4,
        )
        self.pack_ini = self._path_function("pack_ini")
        self.pack_make = self._int_function(
            "pack_make",
            [self._hsp_ptr_int] * 4,
        )
        self.pack_view = self._int_function(
            "pack_view",
            [self._hsp_ptr_int] * 4,
        )
        self.pack_get = self._path_function("pack_get")

    def _int_function(
        self, name: str, argtypes: List[object]
    ) -> Callable[..., int]:
        function = getattr(self._dll, name)
        function.restype = ctypes.c_int
        function.argtypes = argtypes
        return function

    def _path_function(self, name: str) -> Callable[..., int]:
        return self._int_function(
            name,
            [ctypes.c_void_p, ctypes.c_char_p]
            + [self._hsp_ptr_int] * 2,
        )

    def _message(self) -> str:
        message = ctypes.create_string_buffer(32768)
        self.hsc_getmes(message, 0, 0, 0)
        return message.value.decode("mbcs", errors="replace")

    def _check(self, name: str, result: int) -> None:
        if result != 0:
            message = self._message()
            raise RuntimeError("{} failed with {}: {}".format(name, result, message))

    def call_path(self, name: str, path: Path) -> None:
        self._check(name, getattr(self, name)(None, encode_legacy_path(path), 0, 0))

    def close(self) -> None:
        try:
            self.hsc_bye(0, 0, 0, 0)
        finally:
            if self._dll_directory is not None:
                self._dll_directory.close()
                self._dll_directory = None


@contextmanager
def working_directory(path: Path):
    previous = Path.cwd()
    os.chdir(str(path))
    try:
        yield
    finally:
        os.chdir(str(previous))


def test_dll_compile_and_pack(dll_path: Path, common: Path, workdir: Path) -> None:
    dll = HspcmpDll(dll_path)
    try:
        dll_dir = workdir / "DLL-日本語"
        dll_dir.mkdir()
        include = dll_dir / "DLL-含む.as"
        source = dll_dir / "DLL-主.hsp"
        output = dll_dir / "DLL-結果.ax"
        write_fixture(include, 'mes "DLL_INCLUDE_MARKER"\n', "cp932")
        write_fixture(
            source,
            '#include "DLL-含む.as"\nmes "DLL_MAIN_MARKER"\nmes __file__\n',
            "cp932",
        )

        dll.call_path("hsc_ini", source)
        dll.call_path("hsc_compath", common)
        dll.call_path("hsc_objname", output)
        dll._check("hsc_comp", dll.hsc_comp(128, 0, 0, 0))
        result = output.read_bytes()
        require_marker(result, b"DLL_INCLUDE_MARKER", "DLL include marker", output)
        require_marker(result, source.name.encode("utf-8"), "DLL source filename", output)

        pack_dir = workdir / "PACK-日本語"
        pack_dir.mkdir()
        pack_source = pack_dir / "pack-main.hsp"
        asset = pack_dir / "素材-日本語.txt"
        pack_output = pack_dir / "start.ax"
        pack_base = pack_dir / "パック結果"
        asset_contents = b"UNICODE_PACK_ASSET\n"
        asset.write_bytes(asset_contents)
        write_fixture(
            pack_source,
            '#pack "素材-日本語.txt"\nmes "PACK_MAIN_MARKER"\n',
            "cp932",
        )

        with working_directory(pack_dir):
            dll.call_path("hsc_ini", pack_source)
            dll.call_path("hsc_compath", common)
            dll.call_path("hsc_objname", pack_output)
            dll._check("hsc_comp", dll.hsc_comp(128, 4, 0, 0))
            packfile = pack_dir / "packfile"
            if not packfile.is_file():
                raise RuntimeError("DLL compiler did not create packfile")

            dll.call_path("pack_ini", pack_base)
            dll._check("pack_make", dll.pack_make(1, 0, 0, 0))
            dpm = pack_base.with_suffix(".dpm")
            if not dpm.is_file():
                raise RuntimeError("DLL pack maker did not create {}".format(dpm))

            asset.unlink()
            dll.call_path("pack_ini", pack_base)
            dll._check("pack_view", dll.pack_view(0, 0, 0, 0))
            dll.call_path("pack_get", asset)
            if not asset.is_file() or asset.read_bytes() != asset_contents:
                raise RuntimeError("DLL pack extraction did not restore {}".format(asset))
    finally:
        dll.close()


def run_pathio(pathio_exe: Path) -> None:
    run_command([str(pathio_exe)], pathio_exe.parent)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--hspcmp", type=Path, help="path to hspcmp executable")
    parser.add_argument(
        "--common", type=Path, default=ROOT / "common", help="hspcmp common directory"
    )
    parser.add_argument(
        "--hspcmp-dll", type=Path, help="optional Windows hspcmp DLL path"
    )
    parser.add_argument("--pathio-exe", type=Path, help="optional hsp3pathio test executable")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    hspcmp = args.hspcmp or discover_hspcmp()
    hspcmp_dll = args.hspcmp_dll or discover_hspcmp_dll()
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
    if args.hspcmp_dll is not None:
        hspcmp_dll = hspcmp_dll.resolve()
        if not hspcmp_dll.is_file():
            print("hspcmp DLL does not exist: {}".format(hspcmp_dll), file=sys.stderr)
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
            if hspcmp_dll is not None:
                test_dll_compile_and_pack(hspcmp_dll.resolve(), common, workdir)
                print("PASS DLL compile/pack")
            else:
                print("SKIP DLL compile/pack (pass --hspcmp-dll PATH)")
    except (AssertionError, OSError, RuntimeError, UnicodeError) as error:
        print("FAIL: {}".format(error), file=sys.stderr)
        return 1
    print("Unicode path smoke tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
