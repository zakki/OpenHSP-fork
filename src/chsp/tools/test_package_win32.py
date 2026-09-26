from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
import zipfile

import package_win32 as package


class PackageTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.release = package.CHSP / "Release"
        cls.tcc = package.CHSP / "extlib/tcc"
        cls.official = package.ROOT / "dist/hsp37.zip"
        if not cls.official.exists():
            cls.official = None
        cls.files = package.collect(cls.release, cls.tcc, cls.official)

    def test_contents_and_manifest(self):
        manifest = json.loads(self.files["chsp-package.json"])
        self.assertEqual(set(manifest["files"]), set(self.files) - {"chsp-package.json"})
        for name, digest in manifest["files"].items():
            self.assertEqual(digest, package.sha256(self.files[name]), name)
        for forbidden in ("hspcmp.dll", "hspcmp_original.dll", "hspcmp.exe", "hsp3.exe"):
            self.assertNotIn(forbidden, self.files)
        if self.official is not None:
            with zipfile.ZipFile(self.official) as archive:
                self.assertEqual(manifest["official_hspcmp_sha256"],
                                 package.sha256(archive.read("hsp37/hspcmp.dll")))
        else:
            self.assertIsNone(manifest["official_hspcmp_sha256"])
        self.assertEqual((package.ASSETS / "chsp.md").read_bytes(), self.files["doclib/chsp.txt"])
        self.assertEqual({name for name in self.files if name.startswith("doclib/")
                          and not name.startswith("doclib/chsp-license/")}, {"doclib/chsp.txt"})
        self.assertEqual((package.CHSP / "sample/ao_opt.chsp").read_bytes(),
                         self.files["sample/chsp/ao_opt.hsp"])
        help_text = self.files["hsphelp/chsp.hs"].decode("cp932")
        self.assertIn("ネイティブ", help_text)
        self.assertNotIn(b"\n", self.files["hsphelp/chsp.hs"].replace(b"\r\n", b""))

    def test_repeatable_zip_and_failed_build_preserves_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "package.zip"
            _, first = package.build(self.release, self.tcc, self.official, output)
            _, second = package.build(self.release, self.tcc, self.official, output)
            self.assertEqual(first, second)
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.namelist(), sorted("hsp37/" + n for n in self.files))
                self.assertIsNone(archive.testzip())
            with self.assertRaises(OSError):
                package.build(Path(temporary) / "missing", self.tcc, self.official, output)
            self.assertEqual(first, package.sha256(output.read_bytes()))

    def test_rejects_wrong_architecture(self):
        binary = bytearray(self.files["chsp.exe"])
        import struct
        offset = struct.unpack_from("<I", binary, 60)[0]
        struct.pack_into("<H", binary, offset + 4, 0x8664)
        with self.assertRaisesRegex(ValueError, "Win32"):
            package.win32_pe(binary, "wrong.exe")


@unittest.skipUnless(os.name == "nt" and shutil.which("powershell.exe"), "Windows PowerShell required")
class SwitchTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="chsp switch ")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "日本語 space"
        self.root.mkdir()
        for name in ("enable_chsp.bat", "disable_chsp.bat", "switch_chsp.ps1"):
            (self.root / name).write_bytes(package.windows_text(package.ASSETS / name, "ascii"))
        self.original = b"official fixture"
        self.chsp = b"chsp fixture"
        self.active = self.root / "hspcmp.dll"
        self.backup = self.root / "hspcmp_original.dll"
        self.active.write_bytes(self.original)
        (self.root / "hspcmp_chsp.dll").write_bytes(self.chsp)
        (self.root / "chsp-package.json").write_text(json.dumps({
            "format": 1, "official_hspcmp_sha256": package.sha256(self.original),
            "files": {"hspcmp_chsp.dll": package.sha256(self.chsp)},
        }), encoding="ascii")

    def switch(self, mode, success=True):
        result = subprocess.run(["cmd.exe", "/d", "/c", str(self.root / f"{mode}_chsp.bat"), "/quiet"],
                                cwd=self.temporary.name, capture_output=True, timeout=30)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def test_enable_restore_repeat_and_update(self):
        self.switch("disable", False)
        self.assertEqual(self.active.read_bytes(), self.original)
        for _ in range(2):
            self.switch("enable")
            self.switch("enable")
            self.assertEqual(self.active.read_bytes(), self.chsp)
            self.assertEqual(self.backup.read_bytes(), self.original)
            self.switch("disable")
            self.switch("disable")
            self.assertEqual(self.active.read_bytes(), self.original)
            self.assertEqual(self.backup.read_bytes(), self.original)
        updated = b"new chsp fixture"
        (self.root / "hspcmp_chsp.dll").write_bytes(updated)
        manifest_path = self.root / "chsp-package.json"
        manifest = json.loads(manifest_path.read_text())
        manifest["files"]["hspcmp_chsp.dll"] = package.sha256(updated)
        manifest_path.write_text(json.dumps(manifest), encoding="ascii")
        self.switch("enable")
        self.assertEqual(self.active.read_bytes(), updated)
        self.switch("disable")
        self.assertEqual(self.active.read_bytes(), self.original)

    def test_arbitrary_active_can_be_enabled_and_restored(self):
        self.active.write_bytes(b"custom 3.8 active")
        self.switch("enable")
        self.assertEqual(self.active.read_bytes(), self.chsp)
        self.assertEqual(self.backup.read_bytes(), b"custom 3.8 active")
        self.switch("disable")
        self.assertEqual(self.active.read_bytes(), b"custom 3.8 active")

    def test_existing_backup_is_never_overwritten(self):
        self.backup.write_bytes(b"existing backup")
        self.switch("enable", False)
        self.assertEqual(self.active.read_bytes(), self.original)
        self.assertEqual(self.backup.read_bytes(), b"existing backup")
        self.assertEqual(list(self.root.glob("chsp-switch-*.old")), [])

    def test_restore_with_missing_or_damaged_payload(self):
        payload = self.root / "hspcmp_chsp.dll"
        for state in ("missing", "damaged"):
            with self.subTest(state=state):
                payload.write_bytes(self.chsp)
                self.switch("enable")
                if state == "missing":
                    payload.unlink()
                else:
                    payload.write_bytes(b"damaged")
                self.switch("disable")
                self.switch("disable")
                self.assertEqual(self.active.read_bytes(), self.original)
                self.assertEqual(self.backup.read_bytes(), self.original)

    def test_restore_after_overlaying_new_package(self):
        self.switch("enable")
        updated = b"new chsp fixture"
        (self.root / "hspcmp_chsp.dll").write_bytes(updated)
        path = self.root / "chsp-package.json"
        manifest = json.loads(path.read_text())
        manifest["files"]["hspcmp_chsp.dll"] = package.sha256(updated)
        path.write_text(json.dumps(manifest), encoding="ascii")
        self.switch("enable", False)
        self.assertEqual(self.active.read_bytes(), self.chsp)
        self.switch("disable")
        self.assertEqual(self.active.read_bytes(), self.original)
        self.switch("enable")
        self.assertEqual(self.active.read_bytes(), updated)
        self.assertEqual(self.backup.read_bytes(), self.original)

    def test_missing_or_damaged_payload(self):
        payload = self.root / "hspcmp_chsp.dll"
        payload.write_bytes(b"damaged")
        self.switch("enable", False)
        payload.unlink()
        self.switch("enable", False)
        self.assertEqual(self.active.read_bytes(), self.original)
        self.assertFalse(self.backup.exists())

    def test_missing_delegate(self):
        self.active.write_bytes(self.chsp)
        self.switch("enable", False)
        self.switch("disable", False)
        self.assertEqual(self.active.read_bytes(), self.chsp)

    def test_in_use_active_is_untouched(self):
        with self.active.open("rb"):
            self.switch("enable", False)
        self.assertEqual(self.active.read_bytes(), self.original)
        self.assertFalse(self.backup.exists())
        self.assertEqual(list(self.root.glob("chsp-switch-*.tmp")), [])


@unittest.skipUnless(os.name == "nt" and (package.ROOT / "dist/hsp37.zip").exists(),
                     "Windows binaries and official ZIP required")
class RuntimeTest(unittest.TestCase):
    def test_official_overlay_cli_proxy_and_help(self):
        # Current CLI delegation cannot handle spaces in its executable path.
        # The batch switch is tested separately with spaces and Japanese paths.
        with tempfile.TemporaryDirectory(prefix="chsp_runtime_") as temporary:
            root = Path(temporary) / "hsp37"
            root.mkdir()
            with zipfile.ZipFile(package.ROOT / "dist/hsp37.zip") as archive:
                # Only extract the official components used by this smoke test.
                for entry in archive.infolist():
                    name = entry.filename
                    if name.startswith("hsp37/common/") or name in {
                        "hsp37/hspcmp.exe", "hsp37/hspcmp.dll", "hsp37/hsp3cl.exe",
                        "hsp37/runtime/hsp3cl.hrt", "hsp37/hsphelp/i_prep.hs",
                    }:
                        archive.extract(entry, temporary)
            official_hash = package.sha256((root / "hspcmp.dll").read_bytes())
            files = package.collect(package.CHSP / "Release", package.CHSP / "extlib/tcc",
                                    package.ROOT / "dist/hsp37.zip")
            for name, data in files.items():
                destination = root / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
            self.assertEqual(official_hash, package.sha256((root / "hspcmp.dll").read_bytes()))
            env = os.environ.copy()
            for name in ("LIBTCC_DIR", "HSPCMP_ORIGINAL", "CHSP_DEBUG"):
                env.pop(name, None)

            def run(args, cwd=root):
                result = subprocess.run([str(a) for a in args], cwd=cwd, env=env,
                                        capture_output=True, timeout=30)
                output = (result.stdout + result.stderr).decode("cp932", errors="replace")
                self.assertEqual(result.returncode, 0, f"{args}\n{output}")
                return output.replace("\r\n", "\n").strip()

            def compile_source(source, chsp=False, utf8=True):
                args = [root / ("chsp.exe" if chsp else "hspcmp.exe")]
                if chsp:
                    args.append(f"--hspcmp={root / 'hspcmp.exe'}")
                if utf8:
                    args.append("-i")
                args.extend([f"--compath={root.as_posix()}/common/", "-d",
                             f"-o{source.with_suffix('.ax')}", source])
                run(args, cwd=source.parent)
                self.assertTrue(source.with_suffix(".ax").exists())

            for name in package.SAMPLES:
                for variant in ("hsp", "chsp_c", "chsp_p"):
                    source = root / f"sample/chsp_test/{name}_{variant}.hsp"
                    compile_source(source, chsp=True)
                    output = run([root / "hsp3cl.exe", source.with_suffix(".ax")], cwd=source.parent)
                    expected = (source.parent / f"{name}.gt").read_text(encoding="utf-8").strip()
                    self.assertEqual(output, expected, source.name)
            compile_source(root / "sample/chsp/ao_opt.hsp", chsp=True)
            compile_source(root / "sample/chsp/hello.hsp", chsp=True)

            # Exercise the same compiler DLL entry points used by the editor,
            # using the official 32-bit HSPCL runtime as the API host.
            proxy_test = root / "proxy_test.hsp"
            proxy_test.write_text('''#include "hsp3cl.as"
#uselib "hspcmp.dll"
#func proxy_ini "_hsc_ini@16" int, str, int, int
#func proxy_path "_hsc_compath@16" int, str, int, int
#func proxy_comp "_hsc_comp@16" int, int, int, int
#func proxy_mes "_hsc_getmes@16" var, int, int, int
proxy_ini 0, "proxy_input.hsp", 0, 0
proxy_path 0, "common/", 0, 0
proxy_comp 1, 32, 0, 0
result = stat
sdim message, 65536
proxy_mes message, 0, 0, 0
mes message
end result
''', encoding="ascii")
            compile_source(proxy_test)
            run(["cmd.exe", "/d", "/c", str(root / "enable_chsp.bat"), "/quiet"])
            for native in (False, True):
                source = '#include "hsp3cl.as"\nmes 42\nend\n'
                if native:
                    source = (package.ASSETS / "hello.hsp").read_text().replace("stop", "end")
                    source = '#include "hsp3cl.as"\n' + source
                (root / "proxy_input.hsp").write_text(source, encoding="ascii")
                (root / "proxy_input.ax").unlink(missing_ok=True)
                run([root / "hsp3cl.exe", proxy_test.with_suffix(".ax")])
                self.assertTrue((root / "proxy_input.ax").exists())
                self.assertEqual(run([root / "hsp3cl.exe", "proxy_input.ax"]), "42")
            run(["cmd.exe", "/d", "/c", str(root / "disable_chsp.bat"), "/quiet"])
            self.assertEqual(official_hash, package.sha256((root / "hspcmp.dll").read_bytes()))

            # The stock help module must index and open all eight entries.
            help_test = root / "help_test.hsp"
            source = '#include "hsp3cl.as"\n#include "mod_hs.as"\nchdir "hsphelp"\n'
            source += 'sdim message, 4096\nihelp_init message, 0\nif stat != 0 : end 1\n'
            for directive in sorted(package.DIRECTIVES):
                source += f'ihelp_find "{directive}"\nif stat < 1 : end 2\n'
                source += 'ihelp_open 0\nif strlen(ih_info) == 0 : end 3\n'
                source += f'mes "{directive}"\n'
            source += 'end\n'
            help_test.write_text(source, encoding="ascii")
            compile_source(help_test, utf8=False)
            output = run([root / "hsp3cl.exe", help_test.with_suffix(".ax")])
            self.assertEqual(output.splitlines(), sorted(package.DIRECTIVES))


if __name__ == "__main__":
    unittest.main()
