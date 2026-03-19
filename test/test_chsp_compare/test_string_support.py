from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path

from run_test_matrix import EMIT_C_HSPCMP_FLAGS, HSPCMP


class StringSupportTest(unittest.TestCase):
    def run_hspcmp(self, source: Path) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
            cwd=source.parent,
            text=True,
            capture_output=True,
        )

    def test_plugin_target_accepts_string_param_and_return(self) -> None:
        source_text = """\
#chsp_module "str_plugin" target=plugin
#chsp_defcfunc str echo str s
    return s
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "string_plugin.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = self.run_hspcmp(source)

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            native_path = Path(tmpdir) / "str_plugin.c"
            native = native_path.read_text(encoding="utf-8")
            self.assertIn("static char * chsp_func_echo(char * ", native)
            self.assertIn("char * arg_s = code_gets();", native)
            self.assertIn("*type_res = HSPVAR_FLAG_STR;", native)
            self.assertIn("return exinfo->refstr;", native)
            compile_proc = subprocess.run(
                [
                    "cc",
                    "-std=c11",
                    "-I/home/zakki/git/OpenHSP",
                    "-shared",
                    "-fPIC",
                    str(native_path),
                    "-o",
                    str(Path(tmpdir) / "str_plugin.so"),
                    "-lm",
                ],
                text=True,
                capture_output=True,
            )
            self.assertEqual(0, compile_proc.returncode, compile_proc.stdout + compile_proc.stderr)

    def test_c_target_rejects_string_param(self) -> None:
        source_text = """\
#chsp_module "str_c" target=c
#chsp_defcfunc str echo str s
    return s
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "string_c.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = self.run_hspcmp(source)

            self.assertNotEqual(0, proc.returncode)
            self.assertIn("str", proc.stdout + proc.stderr)


if __name__ == "__main__":
    unittest.main()
