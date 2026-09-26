from __future__ import annotations

import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHSP = Path(os.environ.get("CHSP", ROOT / "chsp")).resolve()
UPSTREAM = Path(os.environ.get("HSPCMP_ORIGINAL", ROOT / "hspcmp")).resolve()
RUNTIME = Path(os.environ.get("HSP3CL", ROOT / "hsp3cl")).resolve()
COMPATH = Path(os.environ.get("COMPATH", ROOT / "common")).resolve()


class ChspLibraryTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="chsp_library_")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def invoke(self, args):
        return subprocess.run([str(x) for x in args], cwd=self.directory,
                              capture_output=True, text=True, timeout=30)

    def build(self, name, source, *flags):
        (self.directory / name).write_text(source, encoding="utf-8")
        return self.invoke([CHSP, "--library", f"--compath={COMPATH}/",
                            "--hspcmp=does-not-exist", *flags, name])

    def assert_success(self, proc):
        self.assertEqual(proc.returncode, 0, proc.stdout + proc.stderr)

    def module(self, name, function, value):
        return f'''#chsp_module "{name}"
#chsp_defcfunc {function} int p_value -> int
    return p_value + {value}
#chsp_end
#chsp_module_end
'''

    def test_two_libraries_duplicate_include_and_initialization(self):
        first = 'mes "before"\n' + self.module("first_native", "first_answer", 1)
        first += 'mes first_answer(41)\nmes "after"\n'
        self.assert_success(self.build("first.chsp", first))
        self.assert_success(self.build("second.chsp", self.module("second_native", "second_answer", 2)))
        for name in ("first", "second"):
            self.assertTrue((self.directory / f"{name}.as").exists())
            self.assertFalse((self.directory / f"{name}.ax").exists())
            self.assertFalse((self.directory / f"{name}.chsp.tmp.hsp").exists())
        main = '#include "first.as"\n#include "second.as"\n#include "first.as"\nmes second_answer(40)\nend\n'
        (self.directory / "main.hsp").write_text(main, encoding="utf-8")
        self.assert_success(self.invoke([UPSTREAM, "-i", "-u", f"--compath={COMPATH}/", "-omain.ax", "main.hsp"]))
        result = self.invoke([RUNTIME, "main.ax"])
        self.assert_success(result)
        lines = [line for line in result.stdout.splitlines() if line and line != "gpiod initalize failed."]
        self.assertEqual(lines, ["before", "42", "after", "42"])

    def test_custom_output_and_source_only(self):
        (self.directory / "out").mkdir()
        self.assert_success(self.build("kernel.chsp", self.module("native", "answer", 1),
                                       "-oout/library.as", "--chsp-compile=none"))
        self.assertTrue((self.directory / "out/library.as").exists())
        self.assertTrue((self.directory / "out/native.c").exists())
        self.assertFalse((self.directory / "native.c").exists())
        for suffix in ("dll", "so", "dylib"):
            self.assertFalse((self.directory / f"out/native.{suffix}").exists())

    def test_reject_source_overwrite(self):
        source = self.module("native", "answer", 1)
        result = self.build("kernel.chsp", source, "-okernel.chsp")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("overwrite", result.stderr)
        self.assertEqual((self.directory / "kernel.chsp").read_text(encoding="utf-8"), source)

    def test_plain_hsp_initialization_can_be_packaged(self):
        self.assert_success(self.build("init.hsp", 'mes "init"\n'))
        self.assertIn('mes "init"', (self.directory / "init.as").read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
