#!/usr/bin/env python3
"""cHSP 異常系・エラー検出テストスイート

以下の4つのカテゴリに関するエラー検出と動作検証を行う:
1. 単純な文法エラー (括弧の閉じ忘れ、不正な演算子、ブロックの不整合など)
2. chsp部分での型エラー (不正な型名、未対応の次元数、型構文不正など)
3. chsp部分での非対応命令呼び出し (未対応HSP標準命令、未定義関数呼び出しなど)
4. hsp側への新命令追加を想定したchspディレクティブ外の文法エラー、および動的型境界の挙動
"""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHSP = Path(os.environ.get("CHSP", ROOT / "chsp")).resolve()
HSPCMP = Path(os.environ.get("HSPCMP", ROOT / "hspcmp")).resolve()
HSP3CL = Path(os.environ.get("HSP3CL", ROOT / "hsp3cl")).resolve()
COMPATH = Path(os.environ.get("COMPATH", ROOT / "common")).resolve()


class ChspErrorTestBase(unittest.TestCase):
    """異常系テストの基底クラス"""

    def setUp(self) -> None:
        self.tmpdir = tempfile.mkdtemp(prefix="chsp_err_test_")

    def tearDown(self) -> None:
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def run_chsp_compile(
        self,
        code: str,
        extra_flags: list[str] | None = None,
        source_name: str = "test_case.chsp",
    ) -> tuple[subprocess.CompletedProcess, Path]:
        """コードをファイルに保存して chsp でコンパイルを実行する"""
        source_path = Path(self.tmpdir) / source_name
        source_path.write_text(code, encoding="utf-8")

        cmd = [
            str(CHSP),
            "-i",
            "-u",
            f"--compath={COMPATH}/",
            str(source_path.name),
        ]
        if extra_flags:
            cmd.extend(extra_flags)

        proc = subprocess.run(
            cmd,
            cwd=self.tmpdir,
            capture_output=True,
            text=True,
        )
        return proc, source_path


class ChspSyntaxErrorTest(ChspErrorTestBase):
    """1. 単純な文法エラーのテスト"""

    def test_unclosed_parenthesis(self) -> None:
        """式中の開き括弧が閉じられていないエラー"""
        code = """#chsp_module "syntax_unclosed_paren" target=c
#chsp_defcfunc int compute int a
    return (a + 1
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "開き括弧の閉じ忘れでコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Wrong expression" in combined_out or "error 4" in combined_out,
            f"エラー出力に式エラーが含まれること: {combined_out}",
        )

    def test_consecutive_binary_operators(self) -> None:
        """二項演算子が不正に連続している文法エラー"""
        code = """#chsp_module "syntax_consecutive_ops" target=c
#chsp_defcfunc int compute int a
    return a + * 2
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "不正な演算子連続でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Wrong expression" in combined_out or "error 4" in combined_out,
            f"エラー出力に式エラーが含まれること: {combined_out}",
        )

    def test_unclosed_chsp_module(self) -> None:
        """#chsp_module に対応する #chsp_module_end が欠落しているエラー"""
        code = """#chsp_module "syntax_unclosed_mod" target=c
#chsp_defcfunc int compute int a
    return a + 1
#chsp_end

mes "missing module end"
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "#chsp_module_end 欠落でコンパイルが失敗すること")

    def test_missing_function_name(self) -> None:
        """関数定義で関数名が欠落している文法エラー"""
        code = """#chsp_module "syntax_missing_name" target=c
#chsp_defcfunc int
    return 1
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "関数名欠落でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "error 22" in combined_out or "Only strings are acceptable" in combined_out,
            f"エラー出力に関数名エラーが含まれること: {combined_out}",
        )

    def test_duplicate_function_definition(self) -> None:
        """同一モジュール内で同名関数が重複定義されているエラー"""
        code = """#chsp_module "syntax_dup_fn" target=c
#chsp_defcfunc int compute int a
    return a + 1
#chsp_end
#chsp_defcfunc int compute int a
    return a + 2
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "同名関数の多重定義でコンパイルが失敗すること")


    def test_chsp_directive_outside_module(self) -> None:
        """#chsp_module の外側で #chsp_defcfunc や #chsp_c を呼ぶエラー"""
        code = """#chsp_c {"
static int helper(void) { return 42; }
"}
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "モジュール外の #chsp_c でコンパイルが失敗すること")

    def test_invalid_tokens_statement(self) -> None:
        """構文として成立しない不正なトークン列"""
        code = """#chsp_module "syntax_invalid_tokens" target=c
#chsp_defcfunc int compute int a
    %% && $$
    return a
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "不正トークン文でコンパイルが失敗すること")


class ChspTypeErrorTest(ChspErrorTestBase):
    """2. chsp部分での型エラーのテスト"""

    def test_unknown_return_type(self) -> None:
        """関数定義で未知の戻り値型が指定されたエラー"""
        code = """#chsp_module "type_unknown_ret" target=c
#chsp_defcfunc unknown_type compute int a
    return a
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未知の戻り値型でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "error 23" in combined_out or "Wrong parameter" in combined_out,
            f"パラメータ型エラーが出力されること: {combined_out}",
        )

    def test_unknown_parameter_type(self) -> None:
        """関数引数で未知の型名が指定されたエラー"""
        code = """#chsp_module "type_unknown_param" target=c
#chsp_deffunc compute bad_type x
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未知の引数型でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "error 23" in combined_out or "Wrong parameter" in combined_out,
            f"パラメータ型エラーが出力されること: {combined_out}",
        )

    def test_array_dimension_exceeded(self) -> None:
        """HSPの制限（最大4次元）を超える5次元配列の型注釈エラー"""
        code = """#chsp_module "type_5dim_array" target=c
#chsp_deffunc compute local[int][2][2][2][2][2] arr
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "5次元配列型指定でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "error 23" in combined_out or "Wrong parameter" in combined_out,
            f"エラー出力に型定義エラーが含まれること: {combined_out}",
        )

    def test_invalid_array_type_format(self) -> None:
        """要素型が欠落した不正な配列構文 array[]"""
        code = """#chsp_module "type_invalid_array" target=c
#chsp_deffunc compute array[] arr
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "要素型欠落の配列指定でコンパイルが失敗すること")

    def test_missing_param_separator(self) -> None:
        """引数定義の間でカンマが欠落している型シグネチャエラー"""
        code = """#chsp_module "type_missing_comma" target=c
#chsp_deffunc compute int a int b
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "引数間カンマ欠落でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "error 23" in combined_out or "Wrong parameter" in combined_out,
            f"エラー出力にパラメータエラーが含まれること: {combined_out}",
        )


class ChspUnsupportedCommandTest(ChspErrorTestBase):
    """3. chsp部分での非対応命令呼び出しのテスト"""

    def test_unsupported_hsp_command_mes(self) -> None:
        """cHSP 関数内で非対応の標準命令 mes を呼び出すエラー"""
        code = """#chsp_module "unsupp_mes" target=c
#chsp_deffunc compute
    mes "hello from chsp"
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "cHSP内の非対応命令 mes でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "does not support command" in combined_out or "could not be translated" in combined_out,
            f"非対応命令エラーが出力されること: {combined_out}",
        )

    def test_unsupported_gui_command_pos(self) -> None:
        """cHSP 関数内で非対応のGUI命令 pos を呼び出すエラー"""
        code = """#chsp_module "unsupp_pos" target=c
#chsp_deffunc compute
    pos 100, 200
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "cHSP内の非対応GUI命令 pos でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "does not support command" in combined_out or "could not be translated" in combined_out,
            f"非対応命令エラーが出力されること: {combined_out}",
        )

    def test_unsupported_io_command_picload(self) -> None:
        """cHSP 関数内で非対応の画像ロード命令 picload を呼び出すエラー"""
        code = """#chsp_module "unsupp_picload" target=c
#chsp_deffunc compute
    picload "test.png"
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "cHSP内の非対応命令 picload でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "does not support command" in combined_out or "could not be translated" in combined_out,
            f"非対応命令エラーが出力されること: {combined_out}",
        )

    def test_undefined_function_call(self) -> None:
        """未定義の関数を cHSP 関数内から呼び出すエラー"""
        code = """#chsp_module "unsupp_undef_func" target=c
#chsp_defcfunc int compute int a
    return non_existent_user_func(a)
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未定義関数呼び出しでコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "does not support" in combined_out or "could not be translated" in combined_out,
            f"非対応呼び出しエラーが出力されること: {combined_out}",
        )

    def test_undeclared_native_helper_call(self) -> None:
        """#chsp_c で定義したが #chsp_cdecl なしでネイティブ関数を呼ぶエラー"""
        code = """#chsp_module "unsupp_undecl_native" target=c
#chsp_c {"
static int native_helper(int v) { return v * 2; }
"}
#chsp_defcfunc int compute int v
    return native_helper(v)
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "#chsp_cdecl 欠落のネイティブ関数呼び出しでコンパイルが失敗すること")


class ChspOutsideDirectiveErrorTest(ChspErrorTestBase):
    """4. hsp側への新命令追加を想定したchspディレクティブ外の文法エラーおよびHSP動的型境界のテスト"""

    def test_outside_unknown_future_command(self) -> None:
        """chspディレクティブ外（通常のHSP領域）に未定義の新命令がある場合、hspcmpで検出されること"""
        code = """#chsp_module "valid_module" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end

// chspディレクティブ外の未定義新命令
future_hsp_command_v4 123, 456
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "chspディレクティブ外の未定義命令でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Syntax error" in combined_out or "error 2" in combined_out,
            f"hspcmpのSyntax errorが検出されること: {combined_out}",
        )

    def test_outside_syntax_error_unclosed_paren(self) -> None:
        """chspディレクティブ外での開き括弧閉じ忘れエラー"""
        code = """#chsp_module "valid_module2" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end

mes (1 + 2
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "chspディレクティブ外の構文エラーでコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Wrong expression" in combined_out or "error 4" in combined_out,
            f"hspcmpの式エラーが検出されること: {combined_out}",
        )

    def test_outside_undefined_label(self) -> None:
        """chspディレクティブ外での未定義ラベル参照エラー"""
        code = """#chsp_module "valid_module3" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end

goto *non_existent_label
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未定義ラベル参照でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Label definition not found" in combined_out or "error 13" in combined_out,
            f"hspcmpのラベルエラーが検出されること: {combined_out}",
        )

    def test_line_number_preservation_on_outside_error(self) -> None:
        """空行パディングにより、chspモジュール後方のエラー行番号が元ファイルと正確に一致すること"""
        code = """#chsp_module "valid_mod_pad" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_defcfunc int sub int a, int b
    return a - b
#chsp_end
#chsp_module_end

// 10行目
// 11行目
future_unknown_command_at_line_12 999
"""
        proc, source_path = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode)
        combined_out = proc.stdout + proc.stderr
        # 12行目でエラーが報告されていることを確認
        self.assertRegex(
            combined_out,
            r"\(12\)\s*:\s*error",
            f"元ファイルの12行目に対応するエラー行番号が報告されること: {combined_out}",
        )

    def test_intermediate_cleaned_up_on_error(self) -> None:
        """コンパイルエラー時に中間ファイル (.chsp.tmp.hsp) が残らないこと"""
        code = """#chsp_module "valid_mod_cleanup" target=c
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end

unknown_command_causing_error
"""
        proc, source_path = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode)
        tmp_hsp = source_path.with_name(source_path.stem + ".chsp.tmp.hsp")
        self.assertFalse(
            tmp_hsp.exists(),
            f"エラー時に中間ファイルが削除されていること: {tmp_hsp}",
        )

    def test_dynamic_typing_boundary_runtime_behavior(self) -> None:
        """HSPが動的型のためHSP-cHSP境界の引数型チェックはコンパイル時は限定的で、実行時に型エラーとなることの検証"""
        code = """#include "hsp3cl.as"
#chsp_module "boundary_type_check"
#chsp_defcfunc int add int a, int b
    return a + b
#chsp_end
#chsp_module_end

// int型を期待するcHSP関数に文字列を渡す
res = add("invalid_string", "arg")
mes "res=" + res
"""
        ax_path = Path(self.tmpdir) / "boundary_test.ax"
        proc_comp, source_path = self.run_chsp_compile(
            code,
            extra_flags=["-o" + str(ax_path.name)],
            source_name="boundary_test.chsp",
        )
        # 前提の確認: HSPは動的型のため、コンパイル時チェックは限定的でコンパイル自体は通る
        self.assertEqual(
            0,
            proc_comp.returncode,
            f"動的型のためコンパイル時は通過すること: {proc_comp.stdout}\n{proc_comp.stderr}",
        )

        # 実行時チェック: HSP3CLで実行すると型不一致により実行時エラーで終了すること
        proc_run = subprocess.run(
            [str(HSP3CL), str(ax_path.name)],
            cwd=self.tmpdir,
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(
            0,
            proc_run.returncode,
            "実行時に型不一致エラー（HSPERROR）で異常終了すること",
        )


if __name__ == "__main__":
    unittest.main()
