from __future__ import annotations

import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

THIS_DIR = Path(__file__).resolve().parent
if str(THIS_DIR) not in sys.path:
    sys.path.insert(0, str(THIS_DIR))

from run_test_matrix import EMIT_C_HSPCMP_FLAGS, HSPCMP


ROOT = THIS_DIR.parents[1]
MAKEFILE = ROOT / "makefile"
V2_FRONTEND = ROOT / "src/hspcmp/chsp/chsp_frontend_v2.cpp"
V2_INTERNAL_H = ROOT / "src/hspcmp/chsp/chsp_frontend_v2_internal.h"
V2_EMITTER_CPP = ROOT / "src/hspcmp/chsp/chsp_frontend_v2_emitter.cpp"
V2_PARSER_CPP = ROOT / "src/hspcmp/chsp/chsp_frontend_v2_parser.cpp"
V3_BRIDGE_H = ROOT / "src/hspcmp/chsp/chsp_frontend_v3_bridge.h"
V3_PARSER_CPP = ROOT / "src/hspcmp/chsp/chsp_frontend_v3_parser.cpp"
V3_PARSER_H = ROOT / "src/hspcmp/chsp/chsp_frontend_v3_parser.h"
WIN32_VCXPROJ = ROOT / "src/hspcmp/win32/hspcmp.vcxproj"
WIN32DLL_VCXPROJ = ROOT / "src/hspcmp/win32dll/hspcmp.vcxproj"


class ChspV3ArchitectureGuardrailTest(unittest.TestCase):
    def test_v2_frontend_no_longer_routes_through_parse_program(self) -> None:
        source = V2_FRONTEND.read_text(encoding="utf-8")
        self.assertNotIn("chspv2::ParseProgram(", source)

    def test_v3_parser_does_not_define_parse_body_statement_style_helper(self) -> None:
        parser_sources = (
            V3_PARSER_H.read_text(encoding="utf-8"),
            V3_PARSER_CPP.read_text(encoding="utf-8"),
        )
        for source in parser_sources:
            self.assertNotIn("ParseBodyStatement", source)
            self.assertIsNone(
                re.search(r"\bParse[A-Za-z0-9_]*Statement\s*\(", source),
                "v3 parser should build statements from existing codegen branches",
            )

    def test_v3_parser_declares_ast_builder_state(self) -> None:
        header = V3_PARSER_H.read_text(encoding="utf-8")
        self.assertIn("chsp_frontend_v3_ast.h", header)
        self.assertRegex(header, r"\bChspV3AstProgram\b")
        self.assertRegex(header, r"\bast_program\b")
        self.assertRegex(header, r"\bcurrent_module\b")
        self.assertRegex(header, r"\bcurrent_function\b")
        self.assertRegex(header, r"const\s+chspv3::ChspV3AstProgram\s*&\s*GetAstProgram\s*\(\s*void\s*\)\s*const")

    def test_v3_parser_feeds_ast_builder_from_preprocessor_hooks(self) -> None:
        source = V3_PARSER_CPP.read_text(encoding="utf-8")
        self.assertRegex(source, r"ast_program\.modules\.emplace_back\s*\(")
        self.assertRegex(source, r"current_module->functions\.emplace_back\s*\(")
        self.assertIn("current_function->params.push_back", source)
        self.assertIn("current_function = nullptr;", source)
        self.assertIn("current_module = nullptr;", source)

    def test_v3_parser_does_not_raw_scan_source_for_chsp_forward_registration(self) -> None:
        parser_sources = (
            V3_PARSER_H.read_text(encoding="utf-8"),
            V3_PARSER_CPP.read_text(encoding="utf-8"),
        )
        for source in parser_sources:
            self.assertNotIn("PreRegisterChspFunctionLabels", source)

    def test_v3_parser_feeds_ast_builder_from_statement_and_expr_hooks(self) -> None:
        source = V3_PARSER_CPP.read_text(encoding="utf-8")
        self.assertIn("BeginAstStatement", source)
        self.assertIn("CaptureAstExpr", source)
        self.assertRegex(source, r"GenerateCodeLET[\s\S]*BeginAstStatement")
        self.assertRegex(source, r"GenerateCodeCMD[\s\S]*BeginAstStatement")
        self.assertRegex(source, r"CalcCG_factor[\s\S]*CaptureAstExpr")

    def test_v2_parser_source_is_scheduled_for_removal(self) -> None:
        self.assertFalse(
            V2_PARSER_CPP.exists(),
            "final architecture should remove the v2 parser source from production tree",
        )

    def test_v2_frontend_no_longer_includes_v3_to_v2_bridge(self) -> None:
        source = V2_FRONTEND.read_text(encoding="utf-8")
        self.assertNotIn('#include "chsp_frontend_v3_bridge.h"', source)

    def test_v2_frontend_no_longer_builds_program_via_bridge(self) -> None:
        source = V2_FRONTEND.read_text(encoding="utf-8")
        self.assertNotIn("BuildProgramFromAst(", source)

    def test_v3_bridge_header_is_removed_after_v3_emitter_cutover(self) -> None:
        self.assertFalse(
            V3_BRIDGE_H.exists(),
            "final architecture should delete the v3-to-v2 bridge header",
        )

    def test_v2_emitter_source_is_removed_after_v3_cutover(self) -> None:
        self.assertFalse(
            V2_EMITTER_CPP.exists(),
            "final architecture should delete the v2 emitter source after v3 cutover",
        )

    def test_build_files_do_not_reference_v2_emitter_source(self) -> None:
        for path in (MAKEFILE, WIN32_VCXPROJ, WIN32DLL_VCXPROJ):
            source = path.read_text(encoding="utf-8")
            self.assertNotIn("chsp_frontend_v2_emitter", source)

    def test_v2_internal_header_no_longer_declares_v2_program_pipeline(self) -> None:
        source = V2_INTERNAL_H.read_text(encoding="utf-8")
        self.assertNotIn("struct ChspProgram", source)
        self.assertNotIn("ParseProgram(", source)

    def test_production_sources_do_not_brand_pipeline_as_adapter(self) -> None:
        for path in (V2_FRONTEND, V3_PARSER_H, V3_PARSER_CPP):
            source = path.read_text(encoding="utf-8")
            self.assertNotIn("adapter", source.lower())

    def test_emit_c_smoke_still_generates_native_output_for_chsp_sample(self) -> None:
        source_text = """\
#chsp_module "guardrail_sample" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "guardrail_sample.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            native_path = source.with_suffix(".c")
            self.assertTrue(native_path.exists(), proc.stdout + proc.stderr)
            native = native_path.read_text(encoding="utf-8")
            self.assertIn("Generated by OpenHSP cHSP frontend", native)
            self.assertIn("chsp_func_add", native)

    def test_hspcmp_stdout_includes_v3_ast_json_dump(self) -> None:
        source_text = """\
#chsp_module "json_dump_sample" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "json_dump_sample.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            marker = "#cHSP AST JSON:\n"
            self.assertIn(marker, proc.stdout)
            ast_json = proc.stdout.split(marker, 1)[1].split("\n#cHSP parser log:", 1)[0]
            payload = json.loads(ast_json)
            self.assertEqual("json_dump_sample", payload["modules"][0]["name"])
            self.assertEqual("c", payload["modules"][0]["target"])
            self.assertEqual("add", payload["modules"][0]["functions"][0]["name"])

    def test_hspcmp_ast_json_captures_stmt_kinds_and_assignment_rhs_shape(self) -> None:
        source_text = """\
#chsp_module "shape_sample" target=c
#chsp_defcfunc int sample int a, int b
    value = a + b * 2
    if a : return value : else : return b
    repeat 3
    loop
    return value
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "shape_sample.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            marker = "#cHSP AST JSON:\n"
            ast_json = proc.stdout.split(marker, 1)[1].split("\n#cHSP parser log:", 1)[0]
            payload = json.loads(ast_json)
            body = payload["modules"][0]["functions"][0]["body_stmts"]
            self.assertEqual(["assignment", "if", "repeat", "return"], [stmt["kind"] for stmt in body])
            self.assertEqual(["return", "else"], [stmt["kind"] for stmt in body[1]["children"]])
            self.assertEqual(["return"], [stmt["kind"] for stmt in body[1]["children"][1]["children"]])
            self.assertEqual(["loop"], [stmt["kind"] for stmt in body[2]["children"]])
            self.assertEqual(0, body[0]["if_depth"])
            self.assertEqual(0, body[0]["repeat_depth"])
            self.assertEqual(0, body[1]["if_depth"])
            self.assertGreaterEqual(body[1]["children"][0]["if_depth"], 1)
            self.assertGreaterEqual(body[1]["children"][1]["if_depth"], 1)
            self.assertEqual(0, body[2]["repeat_depth"])
            self.assertGreaterEqual(body[2]["children"][0]["repeat_depth"], 1)

            rhs = body[0]["rhs"]
            self.assertEqual("binary", rhs["kind"])
            self.assertEqual("+", rhs["text"])
            self.assertEqual("identifier", rhs["children"][0]["kind"])
            self.assertEqual("a", rhs["children"][0]["text"])
            self.assertEqual("binary", rhs["children"][1]["kind"])
            self.assertEqual("*", rhs["children"][1]["text"])

    def test_hspcmp_ast_json_and_emit_c_capture_array_access(self) -> None:
        source_text = """\
#chsp_module "array_shape" target=c
#chsp_defcfunc int bump array[int] arr, int i
    arr(i) = arr(0) + 1
    return arr(i)
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "array_shape.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            marker = "#cHSP AST JSON:\n"
            ast_json = proc.stdout.split(marker, 1)[1].split("\n#cHSP parser log:", 1)[0]
            payload = json.loads(ast_json)
            function = payload["modules"][0]["functions"][0]
            self.assertEqual("array[int]", function["params"][0]["type_name"])
            body = function["body_stmts"]

            lhs = body[0]["lhs"]
            self.assertEqual("call", lhs["kind"])
            self.assertEqual("arr", lhs["text"])
            self.assertEqual("identifier", lhs["children"][0]["kind"])
            self.assertEqual("arr", lhs["children"][0]["text"])
            self.assertEqual("identifier", lhs["children"][1]["kind"])
            self.assertEqual("i", lhs["children"][1]["text"])

            rhs = body[0]["rhs"]
            self.assertEqual("binary", rhs["kind"])
            self.assertEqual("call", rhs["children"][0]["kind"])
            self.assertEqual("arr", rhs["children"][0]["text"])
            self.assertEqual("int_literal", rhs["children"][0]["children"][1]["kind"])
            self.assertEqual("0", rhs["children"][0]["children"][1]["text"])

            native = source.with_suffix(".c").read_text(encoding="utf-8")
            self.assertIn("chsp_var_bump_0_arr[chsp_var_bump_1_i] = chsp_var_bump_0_arr[0] + 1;", native)
            self.assertIn("return chsp_var_bump_0_arr[chsp_var_bump_1_i];", native)

    def test_hspcmp_ast_and_emit_c_preserve_nested_array_and_call_exprs(self) -> None:
        source_text = """\
#chsp_module "nested_exprs" target=c
#chsp_defcfunc int pass_value int v
    return v
#chsp_end
#chsp_deffunc write_values array[int] a, local[int] i, local[int] sum
    i = 2
    a.5 = 123
    a.(i + 5) = 9
    sum = a(0) + a.i + a.5 + a.(i + 5)
    a(6) = pass_value(a.(i + 5)) + pass_value(a.i)
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "nested_exprs.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            marker = "#cHSP AST JSON:\n"
            ast_json = proc.stdout.split(marker, 1)[1].split("\n#cHSP parser log:", 1)[0]
            payload = json.loads(ast_json)
            body = payload["modules"][0]["functions"][1]["body_stmts"]

            sum_rhs = body[3]["rhs"]
            self.assertEqual("binary", sum_rhs["kind"])
            self.assertGreaterEqual(len(sum_rhs["children"]), 2)

            assign_rhs = body[4]["rhs"]
            self.assertEqual("binary", assign_rhs["kind"])
            self.assertEqual("call", assign_rhs["children"][0]["kind"])
            self.assertEqual("call", assign_rhs["children"][1]["kind"])

            native = source.with_suffix(".c").read_text(encoding="utf-8")
            self.assertIn("chsp_var_write__values_2_sum = chsp_var_write__values_0_a[0]", native)
            self.assertIn("chsp_func_pass__value(chsp_var_write__values_0_a[chsp_var_write__values_1_i + 5])", native)

    def test_emit_c_normalizes_scoped_builtin_names_in_expr_calls(self) -> None:
        source_text = """\
#chsp_module "builtin_scope" target=c
#chsp_deffunc fill_builtin_args array[int] ints, array[double] reals, local[int[4]] src, local[double[4]] ds, local[int] i
    src(0) = -4
    src.1 = 7
    ds.3 = 9.0
    i = 1
    ints(0) = abs(src.0) + limit(src.i, 0, 10)
    reals(0) = sqrt(absf(ds.3) + 0.25)
    return
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "builtin_scope.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            native = source.with_suffix(".c").read_text(encoding="utf-8")
            self.assertNotIn("@hsp", native)
            self.assertIn("chsp_hsp_abs(", native)
            self.assertIn("chsp_hsp_limit(", native)
            self.assertIn("chsp_hsp_absf(", native)
            self.assertIn("chsp_hsp_sqrt(", native)

    def test_emit_c_preserves_unary_minus_expression_shape(self) -> None:
        source_text = """\
#chsp_module "unary_shape" target=c
#chsp_deffunc compute_vals array[int] vals, local[int] unary_mix
    unary_mix = -(-5) + -(2)
    vals(0) = unary_mix
    return
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "unary_shape.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            marker = "#cHSP AST JSON:\n"
            ast_json = proc.stdout.split(marker, 1)[1].split("\n#cHSP parser log:", 1)[0]
            payload = json.loads(ast_json)
            rhs = payload["modules"][0]["functions"][0]["body_stmts"][0]["rhs"]
            self.assertEqual("binary", rhs["kind"])
            self.assertEqual("unary", rhs["children"][0]["kind"])
            self.assertEqual("unary", rhs["children"][1]["kind"])

            native = source.with_suffix(".c").read_text(encoding="utf-8")
            self.assertNotIn("// unsupported statement", native)
            self.assertIn("chsp_var_compute__vals_1_unary__mix = -(-5) + -(2);", native)

    def test_emit_c_supports_forward_references_to_later_chsp_functions(self) -> None:
        source_text = """\
#chsp_module "forward_refs" target=c
#chsp_deffunc fill_values array[int] out
    out(0) = fib(6)
    forward_fill out
    return
#chsp_end
#chsp_deffunc forward_fill array[int] out
    out(1) = fact(5)
    return
#chsp_end
#chsp_defcfunc int fib int n
    if n <= 1 : return n
    return fib(n - 1) + fib(n - 2)
#chsp_end
#chsp_defcfunc int fact int n
    if n <= 1 : return 1
    return n * fact(n - 1)
#chsp_end
#chsp_module_end
"""
        with tempfile.TemporaryDirectory() as tmpdir:
            source = Path(tmpdir) / "forward_refs.chsp"
            source.write_text(source_text, encoding="utf-8")

            proc = subprocess.run(
                [str(HSPCMP), *EMIT_C_HSPCMP_FLAGS, source.name],
                cwd=source.parent,
                text=True,
                capture_output=True,
            )

            self.assertEqual(0, proc.returncode, proc.stdout + proc.stderr)
            native = source.with_suffix(".c").read_text(encoding="utf-8")
            self.assertIn("chsp_var_fill__values_0_out[0] = chsp_func_fib(6);", native)
            self.assertIn("chsp_func_forward__fill(chsp_var_fill__values_0_out);", native)
            self.assertIn("chsp_var_forward__fill_0_out[1] = chsp_func_fact(5);", native)

if __name__ == "__main__":
    unittest.main()
