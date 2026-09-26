#!/usr/bin/env python3
"""Build the cHSP overlay for the official HSP 3.7 Win32 package."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[3]
CHSP = ROOT / "src/chsp"
ASSETS = CHSP / "package"
DIRECTIVES = {
    "#chsp_module", "#chsp_module_end", "#chsp_deffunc", "#chsp_defcfunc",
    "#chsp_end", "#chsp_c", "#chsp_cdecl", "#chsp_clink",
}
# A small, platform-independent subset of the comparison suite.
SAMPLES = ("scalars", "vectors", "branches", "array_access", "local_arrays")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def win32_pe(data: bytes, name: str) -> None:
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError(f"Not a PE binary: {name}")
    offset = struct.unpack_from("<I", data, 60)[0]
    if (offset + 6 > len(data) or data[offset:offset + 4] != b"PE\0\0"
            or struct.unpack_from("<H", data, offset + 4)[0] != 0x14C):
        raise ValueError(f"Not a Win32 (x86) PE binary: {name}")


def windows_text(path: Path, encoding: str = "cp932") -> bytes:
    return path.read_text(encoding="utf-8-sig").replace("\r\n", "\n").replace(
        "\n", "\r\n").encode(encoding)


def collect(release: Path, tcc: Path, official: Path) -> dict[str, bytes]:
    files: dict[str, bytes] = {}

    def add(name: str, data: bytes) -> None:
        if name in files:
            raise ValueError(f"Duplicate destination: {name}")
        files[name] = data

    def copy(source: Path, name: str) -> None:
        add(name, source.read_bytes())

    def tree(source: Path, name: str) -> None:
        paths = sorted(p for p in source.rglob("*") if p.is_file())
        if not paths:
            raise ValueError(f"Empty or missing input directory: {source}")
        for path in paths:
            if path.is_symlink():
                raise ValueError(f"Symbolic link in input: {path}")
            copy(path, name + "/" + path.relative_to(source).as_posix())

    with zipfile.ZipFile(official) as archive:
        original = archive.read("hsp37/hspcmp.dll")
        win32_pe(original, "official hspcmp.dll")
        for required in ("hsp37/hspcmp.exe", "hsp37/hsed3.exe", "hsp37/common/hspdef.as"):
            archive.getinfo(required)
        official_names = {item.filename.casefold() for item in archive.infolist()}

    for source, destination in (("chsp.exe", "chsp.exe"),
                                ("hspcmp.dll", "hspcmp_chsp.dll"),
                                ("libtcc.dll", "libtcc.dll")):
        data = (release / source).read_bytes()
        win32_pe(data, str(release / source))
        add(destination, data)
    if files["libtcc.dll"] != (tcc / "libtcc.dll").read_bytes():
        raise ValueError("Release/libtcc.dll does not match the supplied TCC runtime")
    if sha256(original) == sha256(files["hspcmp_chsp.dll"]):
        raise ValueError("The cHSP DLL is identical to the official DLL")

    tree(ROOT / "common/chsp", "common/chsp")
    tree(tcc / "include", "tcc/include")
    tree(tcc / "lib", "tcc/lib")
    for required in ("common/chsp/chsp_builtins.tsv", "common/chsp/chsp_runtime.h",
                     "common/chsp/hsp3plugin.h", "tcc/include/stdio.h", "tcc/lib/libtcc1-32.a"):
        if required not in files:
            raise ValueError(f"Missing runtime file: {required}")

    # Deliberately preserve the Markdown bytes; only the extension changes.
    for source, destination in ((CHSP / "docs/chsp.md", "chsp.txt"),
                                (CHSP / "README.md", "chsp-guide.txt"),
                                (CHSP / "docs/chsp-internals.md", "chsp-internals.txt")):
        copy(source, "doclib/" + destination)
    copy(ROOT / "LICENSE", "doclib/chsp-license/LICENSE.OpenHSP")
    copy(ASSETS / "TCC_COPYING", "doclib/chsp-license/TCC_COPYING")
    copy(tcc / "doc/tcc-win32.txt", "doclib/chsp-license/tcc-win32.txt")
    add("README_CHSP.txt", windows_text(ASSETS / "README_CHSP.txt"))
    for name in ("enable_chsp.bat", "disable_chsp.bat", "switch_chsp.ps1"):
        add(name, windows_text(ASSETS / name, "ascii"))

    help_source = ASSETS / "chsp.hs"
    indices = re.findall(r"^%index\s*\n([^\n]+)", help_source.read_text(encoding="utf-8"), re.M)
    if set(indices) != DIRECTIVES or len(indices) != len(DIRECTIVES):
        raise ValueError("Help must document each cHSP directive exactly once")
    add("hsphelp/chsp.hs", windows_text(help_source))
    # ao_opt.hsp in the source tree is old generated output, not the input.
    copy(CHSP / "sample/ao_opt.chsp", "sample/chsp/ao_opt.hsp")
    copy(CHSP / "sample/ao_original.hsp", "sample/chsp/ao_original.hsp")
    copy(ASSETS / "hello.hsp", "sample/chsp/hello.hsp")

    generator_path = ROOT / "test/test_chsp_compare/generate_templates.py"
    spec = importlib.util.spec_from_file_location("chsp_package_templates", generator_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    for name in SAMPLES:
        rendered = module.render_template(generator_path.parent / (name + ".template"))
        for variant in ("hsp", "chsp_c", "chsp_p"):
            add(f"sample/chsp_test/{name}_{variant}.hsp", rendered[variant].encode("utf-8"))
        copy(generator_path.parent / (name + ".gt"), f"sample/chsp_test/{name}.gt")

    manifest = {
        "format": 1,
        "target": "HSP 3.7 Win32",
        "official_hspcmp_sha256": sha256(original),
        "files": {name: sha256(data) for name, data in sorted(files.items())},
    }
    add("chsp-package.json", (json.dumps(manifest, indent=2) + "\n").encode("ascii"))
    for name in files:
        if "hsp37/" + name.casefold() in official_names:
            raise ValueError(f"Package would overwrite an official file: {name}")
    return files


def build(release: Path, tcc: Path, official: Path, output: Path) -> tuple[int, str]:
    # Gather and validate everything before touching an existing output ZIP.
    files = collect(release, tcc, official)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="chsp-package-", dir=output.parent) as temporary:
        pending = Path(temporary) / "package.zip"
        with zipfile.ZipFile(pending, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, data in sorted(files.items()):
                info = zipfile.ZipInfo("hsp37/" + name, date_time=(2026, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_DEFLATED
                info.create_system = 3
                info.external_attr = 0o100644 << 16
                archive.writestr(info, data)
        with zipfile.ZipFile(pending) as archive:
            if archive.testzip() is not None:
                raise ValueError("ZIP integrity check failed")
        digest = sha256(pending.read_bytes())
        pending.replace(output)
    return len(files), digest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--release-dir", type=Path, default=CHSP / "Release")
    parser.add_argument("--tcc-dir", type=Path, default=CHSP / "extlib/tcc")
    parser.add_argument("--official-zip", type=Path, default=ROOT / "dist/hsp37.zip")
    parser.add_argument("--output", type=Path, default=ROOT / "dist/chsp_hsp37_win32.zip")
    args = parser.parse_args()
    try:
        if args.output.resolve() == args.official_zip.resolve():
            raise ValueError("Output must not replace the official ZIP")
        count, digest = build(args.release_dir, args.tcc_dir, args.official_zip, args.output)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f"Packaging failed: {error}\n")
    print(f"Created {args.output} ({count} files)\nSHA256 {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
