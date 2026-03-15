from __future__ import annotations

import subprocess
import tempfile
import unittest

from pathlib import Path

from generate_templates import parse_template, render_template
from run_test_matrix import (
    TEMPLATE_CASES,
    compare_variants_for_case,
    compiled_artifact_paths,
    emitted_artifact_paths,
    hspcmp_flags,
    shared_library_cflags_for_variant,
    variants_for_mode,
)


class RunTestMatrixConfigTest(unittest.TestCase):
    def test_help_describes_mode_and_variants(self) -> None:
        script = Path(__file__).with_name("run_test_matrix.py")
        proc = subprocess.run(
            ["python3", str(script), "compare", "--help"],
            text=True,
            capture_output=True,
            check=True,
        )
        self.assertIn("mode selects the native compile path", proc.stdout)
        self.assertIn("variant selects which generated", proc.stdout)
        self.assertIn("source kind from a template is executed", proc.stdout)
        self.assertIn("default", proc.stdout)
        self.assertIn("emit-c", proc.stdout)
        self.assertIn("libtcc", proc.stdout)

    def test_template_cases_follow_templates_with_gt(self) -> None:
        self.assertIn("mixed_numeric_return", TEMPLATE_CASES)
        self.assertIn("operators", TEMPLATE_CASES)
        self.assertIn("recursive_calls", TEMPLATE_CASES)
        self.assertIn("trig", TEMPLATE_CASES)
        self.assertIn("user_function_calls", TEMPLATE_CASES)
        self.assertNotIn("double_return", TEMPLATE_CASES)

    def test_variants_for_mode(self) -> None:
        self.assertEqual(("hsp", "chsp_p", "chsp_c"), variants_for_mode("default"))
        self.assertEqual(("chsp_p", "chsp_c"), variants_for_mode("emit-c"))
        self.assertEqual(("chsp_p", "chsp_c"), variants_for_mode("libtcc"))

    def test_compare_variants_can_be_restricted_per_case(self) -> None:
        self.assertEqual(("chsp_p",), compare_variants_for_case("mixed_numeric_return", "default"))
        self.assertEqual(("chsp_p",), compare_variants_for_case("mixed_numeric_return", "emit-c"))
        self.assertEqual(("chsp_p",), compare_variants_for_case("mixed_numeric_return", "libtcc"))

    def test_hspcmp_flags(self) -> None:
        self.assertNotIn("--chsp-compile=none", hspcmp_flags("default"))
        self.assertIn("--chsp-compile=none", hspcmp_flags("emit-c"))
        self.assertIn("--chsp-compile=libtcc", hspcmp_flags("libtcc"))

    def test_compiled_artifact_paths_for_c_target_variant(self) -> None:
        c_path, so_path = compiled_artifact_paths(Path("/tmp/array_access_chsp_c.hsp"), "chsp_c")
        self.assertEqual(Path("/tmp/array_access_chsp_c.c"), c_path)
        self.assertEqual(Path("/tmp/array_access_chsp_c.so"), so_path)

    def test_emitted_artifact_paths_follow_chsp_module_name_for_plugin_variant(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "mixed_numeric_return_chsp_p.hsp"
            source.write_text(
                '#chsp_module "mixed_numeric_return"\n#chsp_module_end\n',
                encoding="utf-8",
            )
            c_path, so_path = emitted_artifact_paths(source, "chsp_p")
            self.assertEqual(Path(tmpdir) / "mixed_numeric_return.c", c_path)
            self.assertEqual(Path(tmpdir) / "mixed_numeric_return.so", so_path)

    def test_shared_library_cflags_include_plugin_runtime_defines_for_chsp_p(self) -> None:
        self.assertNotIn("-DHSPLINUX", shared_library_cflags_for_variant("chsp_c"))
        self.assertNotIn("-DHSP64", shared_library_cflags_for_variant("chsp_c"))
        self.assertIn("-DHSPLINUX", shared_library_cflags_for_variant("chsp_p"))
        self.assertIn("-DHSP64", shared_library_cflags_for_variant("chsp_p"))

    def test_template_meta_allows_partial_top_level_sections(self) -> None:
        template = """@@ meta
compare_default = hsp, chsp_p
compare_emit_c = chsp_p

@@ header
#include "hsp3cl.as"

@@ hsp
{{header}}

@@ chsp_p
{{header}}
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "partial.template"
            path.write_text(template, encoding="utf-8")
            spec = parse_template(path)
            self.assertEqual(("hsp", "chsp_p"), spec.top_level_sections)
            self.assertEqual(("hsp", "chsp_p"), spec.compare_variants["default"])
            self.assertEqual(("chsp_p",), spec.compare_variants["emit-c"])
            rendered = render_template(path)
            self.assertEqual({"hsp", "chsp_p"}, set(rendered))


if __name__ == "__main__":
    unittest.main()
