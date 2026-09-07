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
#chsp_defcfunc compute int a -> int
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
#chsp_defcfunc compute int a -> int
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
#chsp_defcfunc compute int a -> int
    return a + 1
#chsp_end

mes "missing module end"
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "#chsp_module_end 欠落でコンパイルが失敗すること")

    def test_missing_function_name(self) -> None:
        """関数定義で関数名が欠落している文法エラー"""
        code = """#chsp_module "syntax_missing_name" target=c
#chsp_defcfunc -> int
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
#chsp_defcfunc compute int a -> int
    return a + 1
#chsp_end
#chsp_defcfunc compute int a -> int
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
#chsp_defcfunc compute int a -> int
    %% && $$
    return a
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "不正トークン文でコンパイルが失敗すること")


class ChspTypeErrorTest(ChspErrorTestBase):
    """2. chsp部分での型エラーのテスト"""


    def test_missing_return_type(self) -> None:
        """#chsp_defcfunc で -> <type> が欠落しているエラー"""
        code = """#chsp_module "syntax_missing_ret" target=c
#chsp_defcfunc compute int a
    return a
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "戻り値型欠落でコンパイルが失敗すること")

    def test_legacy_syntax_rejected(self) -> None:
        """旧記法 #chsp_defcfunc int compute がエラーとして拒絶されること"""
        code = """#chsp_module "syntax_legacy_rejected" target=c
#chsp_defcfunc int compute int a
    return a
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "旧記法のdefcfuncでコンパイルが失敗すること")

    def test_unknown_return_type(self) -> None:
        """関数定義で未知の戻り値型が指定されたエラー"""
        code = """#chsp_module "type_unknown_ret" target=c
#chsp_defcfunc compute int a -> unknown_type
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
#chsp_defcfunc compute int a -> int
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
#chsp_defcfunc compute int v -> int
    return native_helper(v)
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "#chsp_cdecl 欠落のネイティブ関数呼び出しでコンパイルが失敗すること")


class ChspDimCommandErrorTest(ChspErrorTestBase):
    """dim / ddim / lldim / dimtype 命令の異常系テスト"""

    def test_dim_on_target_c_unsupported(self) -> None:
        """target=c で dim 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "dim_target_c" target=c
#chsp_deffunc resize array[int] a
    dim a, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "target=c での dim でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "only supported in target=plugin" in combined_out,
            f"target=plugin 限定エラーが出力されること: {combined_out}",
        )

    def test_dim_on_local_array_unsupported(self) -> None:
        """local 配列に対して dim 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "dim_local"
#chsp_deffunc resize local[int[4]] a
    dim a, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "local配列に対する dim でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "cannot be used on local array" in combined_out,
            f"local配列エラーが出力されること: {combined_out}",
        )

    def test_dim_type_mismatch(self) -> None:
        """型不一致の配列に対して dim 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "dim_mismatch"
#chsp_deffunc resize array[double] d
    dim d, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "型不一致の dim でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "type mismatch" in combined_out,
            f"型不一致エラーが出力されること: {combined_out}",
        )

    def test_ddim_type_mismatch(self) -> None:
        """型不一致の配列に対して ddim 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "ddim_mismatch"
#chsp_deffunc resize array[int] a
    ddim a, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "型不一致の ddim でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "type mismatch" in combined_out,
            f"型不一致エラーが出力されること: {combined_out}",
        )

    def test_lldim_type_mismatch(self) -> None:
        """型不一致の配列に対して lldim 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "lldim_mismatch"
#chsp_deffunc resize array[int] a
    lldim a, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "型不一致の lldim でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "type mismatch" in combined_out,
            f"型不一致エラーが出力されること: {combined_out}",
        )

    def test_dimtype_dynamic_expression_unsupported(self) -> None:
        """dimtype の型指定に動的変数を指定するとコンパイルエラーになること"""
        code = """#chsp_module "dimtype_dynamic"
#chsp_deffunc resize array[int] a, int t
    dimtype a, t, 10
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "動的型 dimtype でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "constant integer" in combined_out,
            f"定数整数制限エラーが出力されること: {combined_out}",
        )


class ChspLabelGosubErrorTest(ChspErrorTestBase):
    """label 引数型および gosub コマンドの異常系テスト"""

    def test_label_param_on_target_c_unsupported(self) -> None:
        """target=c で label 引数型を使用するとコンパイルエラーになること"""
        code = """#chsp_module "label_target_c" target=c
#chsp_deffunc callback_runner label cb
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "target=c での label 型引数でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "label parameters are only supported in target=plugin" in combined_out,
            f"target=plugin 限定エラーが出力されること: {combined_out}",
        )

    def test_gosub_on_target_c_unsupported(self) -> None:
        """target=c で gosub 命令を使用するとコンパイルエラーになること"""
        code = """#chsp_module "gosub_target_c" target=c
#chsp_deffunc runner int a
    gosub a
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "target=c での gosub でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "gosub is only supported in target=plugin" in combined_out,
            f"target=plugin 限定エラーが出力されること: {combined_out}",
        )

    def test_gosub_missing_argument(self) -> None:
        """gosub 命令に引数が指定されていない場合コンパイルエラーになること"""
        code = """#chsp_module "gosub_no_arg"
#chsp_deffunc runner
    gosub
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "引数なし gosub でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "gosub requires a label identifier argument" in combined_out,
            f"引数要求エラーが出力されること: {combined_out}",
        )

    def test_gosub_unknown_variable(self) -> None:
        """引数リストに存在しない変数名を gosub に指定するとエラーになること"""
        code = """#chsp_module "gosub_unknown_var"
#chsp_deffunc runner label cb
    gosub unknown_cb
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未定義変数 gosub でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "not found in function parameters" in combined_out,
            f"パラメータ未発見エラーが出力されること: {combined_out}",
        )

    def test_gosub_type_mismatch(self) -> None:
        """label 型以外の引数を gosub に指定するとエラーになること"""
        code = """#chsp_module "gosub_type_mismatch"
#chsp_deffunc runner int a
    gosub a
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "型不一致 gosub でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "type mismatch: gosub requires label" in combined_out,
            f"型不一致エラーが出力されること: {combined_out}",
        )

    def test_gosub_on_local_variable(self) -> None:
        """local 変数を gosub に指定するとエラーになること"""
        code = """#chsp_module "gosub_local"
#chsp_deffunc runner local[int] a
    gosub a
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "local変数に対する gosub でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "cannot be used on local variable" in combined_out,
            f"local変数エラーが出力されること: {combined_out}",
        )

    def test_callback_termination_propagation_codegen(self) -> None:
        """コールバック中断(RUNMODE_END)がネイティブ呼び出し元、代入、if、repeat、ディスパッチ関数へ正しく伝搬するコードが生成されること"""
        code = """#include "hsp3cl.as"
#chsp_module "term_codegen"
#chsp_defcfunc helper_fn label cb -> int
    gosub cb
    return 10
#chsp_end

#chsp_deffunc helper_cmd label cb
    gosub cb
    return
#chsp_end

#chsp_deffunc caller_cmd label cb
    helper_cmd cb
    return
#chsp_end

#chsp_deffunc caller_assign label cb, local[int] x
    x = helper_fn(cb)
    return
#chsp_end

#chsp_deffunc caller_if label cb
    if ( helper_fn(cb) > 0 ) {
        helper_cmd cb
    }
    return
#chsp_end

#chsp_deffunc caller_repeat label cb
    repeat helper_fn(cb)
        helper_cmd cb
    loop
    return
#chsp_end
#chsp_module_end
"""
        proc, source_path = self.run_chsp_compile(code, extra_flags=["--chsp-compile=none"])
        self.assertEqual(0, proc.returncode, f"コンパイルが成功すること: {proc.stdout}\n{proc.stderr}")
        c_path = Path(self.tmpdir) / "term_codegen.c"
        self.assertTrue(c_path.exists(), f"生成されたCソースが存在すること: {c_path}")
        c_code = c_path.read_text(encoding="utf-8")

        # 1. 各関数のエントリガード
        self.assertIn("if ( ctx->runmode == RUNMODE_END ) return 0;", c_code)
        self.assertIn("if ( ctx->runmode == RUNMODE_END ) return;", c_code)

        # 2. caller_cmd 内で helper_cmd 呼び出し直後のガード
        self.assertRegex(
            c_code,
            r"chsp_func_helper__cmd\([^)]*\);\s*if\s*\(\s*ctx->runmode\s*==\s*RUNMODE_END\s*\)\s*return;",
        )

        # 3. caller_assign 内で helper_fn 呼び出し代入直後のガード
        self.assertRegex(
            c_code,
            r"chsp_var_caller__assign_\d+_x\s*=\s*chsp_func_helper__fn\([^)]*\);\s*if\s*\(\s*ctx->runmode\s*==\s*RUNMODE_END\s*\)\s*return;",
        )

        # 4. caller_if 内で条件式の一時変数評価と分岐前のガード
        self.assertRegex(
            c_code,
            r"int\s+_chsp_cond_\d+\s*=\s*\(chsp_func_helper__fn\([^)]*\)\s*>\s*0\);\s*if\s*\(\s*ctx->runmode\s*==\s*RUNMODE_END\s*\)\s*return;\s*if\s*\(_chsp_cond_\d+\)",
        )

        # 5. caller_repeat 内でループ回数の一時変数評価とループ前のガード
        self.assertRegex(
            c_code,
            r"int\s+_chsp_cnt_max_\d+\s*=\s*chsp_func_helper__fn\([^)]*\);\s*if\s*\(\s*ctx->runmode\s*==\s*RUNMODE_END\s*\)\s*return;",
        )

        # 6. cmdfunc で return ctx->runmode
        self.assertIn("return ctx->runmode;", c_code)

        # 7. reffunc での RUNMODE_END ガード
        self.assertIn("if ( ctx->runmode == RUNMODE_END ) {\n        puterror( HSPERR_NONE );", c_code)

    def test_callback_termination_propagation_runtime(self) -> None:
        """コールバックで end が実行された際、ネストした呼び出し元関数が中断を正しく伝搬して後続文を実行しないこと"""
        code = """#include "hsp3cl.as"
#chsp_module "term_runtime"
#chsp_deffunc helper array[int] flag, label cb
    gosub cb
    flag(0) = 999
    return
#chsp_end

#chsp_deffunc caller array[int] flag, label cb
    helper flag, cb
    flag(1) = 888
    return
#chsp_end
#chsp_module_end

dim flag, 2
flag(0) = 10
flag(1) = 20

mes "START"
caller flag, *on_cb
mes "AFTER_CALLER"
end

*on_cb
mes "IN_CALLBACK_TERMINATING"
end
"""
        ax_path = Path(self.tmpdir) / "term_runtime.ax"
        c_path = Path(self.tmpdir) / "term_runtime.c"
        so_path = Path(self.tmpdir) / "term_runtime.so"

        # Cソース生成
        proc, _ = self.run_chsp_compile(
            code,
            extra_flags=["--chsp-compile=none", "-o" + str(ax_path.name)],
            source_name="term_runtime.chsp",
        )
        self.assertEqual(0, proc.returncode, f"コンパイル成功: {proc.stdout}\n{proc.stderr}")

        # gcc で共有ライブラリをビルド
        build_cmd = ["gcc", "-std=c11", "-shared", "-fPIC", f"-I{ROOT}", "-o", str(so_path), str(c_path)]
        build_res = subprocess.run(build_cmd, capture_output=True, text=True)
        self.assertEqual(0, build_res.returncode, f"gccビルド成功: {build_res.stdout}\n{build_res.stderr}")

        # hsp3cl で実行
        env = os.environ.copy()
        env["LD_LIBRARY_PATH"] = str(self.tmpdir)
        run_res = subprocess.run(
            [str(HSP3CL), str(ax_path.name)],
            cwd=self.tmpdir,
            capture_output=True,
            text=True,
            env=env,
        )
        self.assertEqual(0, run_res.returncode, f"正常終了すること: {run_res.stdout}\n{run_res.stderr}")
        combined_run = run_res.stdout + run_res.stderr
        self.assertIn("START", combined_run)
        self.assertIn("IN_CALLBACK_TERMINATING", combined_run)
        # 後続文が実行されていないこと
        self.assertNotIn("AFTER_CALLER", combined_run)


class ChspHsp64AbiTest(ChspErrorTestBase):
    """ホストランタイムの明示的なHSP64 ABI選択が保持され、ポインタ幅のみから導出されないことのテスト"""

    def test_plugin_preamble_does_not_derive_hsp64_from_pointer_width(self) -> None:
        """生成されるプラグイン用Cソースのpreambleが、ポインタ幅/アーキテクチャ判定(__x86_64__等)からHSP64を強制定義しないこと"""
        code = """#include "hsp3cl.as"
#chsp_module "hsp64_abi_test"
#chsp_deffunc dummy
    return
#chsp_end
#chsp_module_end
"""
        proc, _ = self.run_chsp_compile(code, extra_flags=["--chsp-compile=none"])
        self.assertEqual(0, proc.returncode, f"コンパイル成功: {proc.stdout}\n{proc.stderr}")
        c_path = Path(self.tmpdir) / "hsp64_abi_test.c"
        self.assertTrue(c_path.exists(), f"生成されたCソースが存在すること: {c_path}")
        c_code = c_path.read_text(encoding="utf-8")

        # アーキテクチャやポインタ幅判定マクロからHSP64を強制定義していないこと
        self.assertNotIn("__x86_64__", c_code)
        self.assertNotIn("_M_X64", c_code)
        self.assertNotIn("__aarch64__", c_code)
        self.assertNotIn("__UINTPTR_MAX__", c_code)

        # 現在のビルド環境 (HSP64が有効) では明示的な #define HSP64 が出力されること
        self.assertIn("#ifndef HSP64\n#define HSP64\n#endif", c_code)

    def test_chsp_c_emitter_non_hsp64_build_undefines_hsp64(self) -> None:
        """HSP64が無効なホスト環境ビルドでは、生成コードでHSP64が定義されず未定義化されること"""
        obj_path = Path(self.tmpdir) / "emitter_no_hsp64.o"
        compile_cmd = [
            "g++",
            "-Wno-write-strings",
            "-std=c++17",
            "--exec-charset=UTF-8",
            "-DHSPLINUX",
            "-DHSPDEBUG",
            "-DHSP_COM_UNSUPPORTED",
            "-Werror=int-to-pointer-cast",
            "-Isrc/chsp",
            "-Isrc/hspcmp",
            "-Isrc/hsp3",
            "-c",
            "src/chsp/chsp_c_emitter.cpp",
            "-o",
            str(obj_path),
        ]
        res = subprocess.run(compile_cmd, cwd=ROOT, capture_output=True, text=True)
        self.assertEqual(0, res.returncode, f"非HSP64でのemitterコンパイルが成功すること: {res.stderr}")

        strings_out = subprocess.check_output(["strings", str(obj_path)]).decode("latin-1")
        # 非HSP64ビルドでは #undef HSP64 が出力対象文字列となり、#define HSP64 や __x86_64__ は含まれない
        self.assertIn("#undef HSP64", strings_out)
        self.assertNotIn("#define HSP64", strings_out)
        self.assertNotIn("__x86_64__", strings_out)


class ChspOutsideDirectiveErrorTest(ChspErrorTestBase):
    """4. hsp側への新命令追加を想定したchspディレクティブ外の文法エラーおよびHSP動的型境界のテスト"""

    def test_outside_unknown_future_command(self) -> None:
        """chspディレクティブ外（通常のHSP領域）に未定義の新命令がある場合、hspcmpで検出されること"""
        code = """#chsp_module "valid_module" target=c
#chsp_defcfunc add int a, int b -> int
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
#chsp_defcfunc add int a, int b -> int
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
#chsp_defcfunc add int a, int b -> int
    return a + b
#chsp_end
#chsp_module_end

goto *non_existent_label
"""
        proc, _ = self.run_chsp_compile(code)
        self.assertNotEqual(0, proc.returncode, "未定義ラベル参照でコンパイルが失敗すること")
        combined_out = proc.stdout + proc.stderr
        self.assertTrue(
            "Label definition not found" in combined_out
            or "ラベルの定義が存在しません" in combined_out
            or "error 13" in combined_out,
            f"hspcmpのラベルエラーが検出されること: {combined_out}",
        )

    def test_line_number_preservation_on_outside_error(self) -> None:
        """空行パディングにより、chspモジュール後方のエラー行番号が元ファイルと正確に一致すること"""
        code = """#chsp_module "valid_mod_pad" target=c
#chsp_defcfunc add int a, int b -> int
    return a + b
#chsp_end
#chsp_defcfunc sub int a, int b -> int
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
#chsp_defcfunc add int a, int b -> int
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
#chsp_defcfunc add int a, int b -> int
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
