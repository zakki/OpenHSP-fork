# chsp compare test テンプレート作成ガイド

## 目的

`test/test_chsp_compare` では、同じテスト本体から必要なバリアントだけを生成し、比較できるようにすることを想定する。

- `test_a.hsp`
- `test_a_chsp_c.hsp`
- `test_a_chsp_p.hsp`

狙いは次の 2 点。

- テストの共通部分を 1 か所に寄せて、修正漏れを減らす
- `vanilla HSP` / `target=c` / `target=plugin` の差分を、モジュール定義の差だけに寄せて見やすくする

このドキュメントは、実装前に合意しておく簡易仕様と、典型的なテストコードの書き方をまとめたもの。

## 想定するファイル構成

テンプレート 1 個から、同じディレクトリに必要なファイルを生成する。

```text
test_x.template
gen/test_x.hsp
gen/test_x_chsp_c.hsp
gen/test_x_chsp_p.hsp
```

命名ルールは以下。

- `test_x.hsp`: vanilla HSP 用
- `test_x_chsp_c.hsp`: `#chsp_module ... target=c` のCターゲット用
- `test_x_chsp_p.hsp`: `#chsp_module ...` のプラグインターゲット用

テンプレートによっては、`hsp` と `chsp_p` だけ、あるいは `chsp_p` だけを生成してもよい。

## テンプレートの基本仕様

テンプレートは section ベースのプレーンテキストとし、`@@ section_name` で区切る。

```text
@@ meta
compare_default = hsp, chsp_p
compare_emit_c = chsp_p
compare_libtcc = chsp_p

@@ header
...

@@ hsp_module
...

@@ chsp_c_module
...

@@ chsp_p_module
...

@@ main
...

@@ hsp
{{header}}
{{hsp_module}}
{{main}}

@@ chsp_p
{{header}}
{{chsp_p_module}}
{{main}}
```

仕様の要点は以下。

- `{{section_name}}` で別 section を参照できる
- `meta` は任意で、compare 時の対象バリアントを mode ごとに指定できる
- 最上位 section は `hsp` / `chsp_c` / `chsp_p` のうち必要なものだけ置ける
- 共通化したいヘッダー、補助関数、呼び出し部は別 section に切り出す
- 差分を持たせたい箇所は `hsp_module` / `chsp_c_module` / `chsp_p_module` に分ける

`meta` で使えるキーは以下。

- `compare_default`
- `compare_emit_c`
- `compare_libtcc`

値は `hsp`, `chsp_c`, `chsp_p` のカンマ区切りまたは空白区切り。省略した mode では、その mode で通常比較されるバリアントのうち、実際に top-level section が存在するものだけが比較対象になる。

## section の役割

典型的には次の切り分けを推奨する。

- `header`
  - `#include`、共通マクロ、テスト全体で使う初期化
- `hsp_module`
  - vanilla HSP 側の `#module` / `#deffunc` / `#global`
- `chsp_c_module`
  - `target=c` 向けの `#chsp_module`
- `chsp_p_module`
  - plugin 向けの `#chsp_module`
- `main`
  - 入出力、関数呼び出し、比較対象になる実行部

この分割にすると、差分レビュー時に「モジュール定義の違い」と「テストシナリオ本体」を分離できる。

## 典型例

```text
@@ header
#include "hsp3cl.as"

@@ hsp_module
#module

#deffunc add_and_print int a, int b
	mes "" + (a + b)
	return

#deffunc dummy_0
	return

#global

@@ chsp_c_module
#chsp_module "test_add" target=c

#chsp_deffunc add_and_print int a, int b
	mes "" + (a + b)
	return
#chsp_end

#deffunc dummy_0
	return

#chsp_module_end

@@ chsp_p_module
#chsp_module "test_add"

#chsp_deffunc add_and_print int a, int b
	mes "" + (a + b)
	return
#chsp_end

#deffunc dummy_0
	return

#chsp_module_end

@@ main
	mes "BEGIN"
	add_and_print 10, 20
	mes "END"
	end

	dummy_0

@@ hsp
{{header}}
{{hsp_module}}
{{main}}

@@ chsp_c
{{header}}
{{chsp_c_module}}
{{main}}

@@ chsp_p
{{header}}
{{chsp_p_module}}
{{main}}
```

このテンプレートからは、概念的に次の 3 ファイルが生成される。

- `test_add.hsp`
- `test_add_chsp_c.hsp`
- `test_add_chsp_p.hsp`

## 典型的なテストの書き方

比較しやすいテストにするため、以下を推奨する。

- 出力は `mes` 中心で固定し、空白や改行の揺れを減らす
- 乱数や時刻など非決定要素は避けるか固定する
- テスト本体の呼び出し順は `hsp` / `chsp_c` / `chsp_p` で完全に一致させる
- 差分はモジュール宣言と型宣言に閉じ込める
- 補助関数が必要なら `header` ではなく各 `*_module` 側に置き、スコープを明確にする

## 向いているケース

- 単純な算術
- 配列アクセス
- 分岐
- ループ
- 組み込み関数呼び出し
- `#chsp_deffunc` と `#deffunc` の対応確認

## 向いていないケース

以下はテンプレート 1 枚で無理に共通化しない方がよい。

- 3 バリアントで呼び出し方法自体が大きく変わるテスト
- target ごとに前提ライブラリや初期化手順が異なるテスト
- 出力より生成物や変換結果の差を主に見たいテスト

その場合は、従来どおり個別ファイルで持つ方が読みやすい。

## 作成手順

1. `test_x.template` を作る
2. まず `header`、`main` を書いて共通部分を固める
3. `hsp_module`、`chsp_c_module`、`chsp_p_module` を最小差分で書く
4. `hsp`、`chsp_c`、`chsp_p` の最上位 section で組み立てる
5. 必要なら `meta` を追加して、mode ごとの比較対象を絞る
6. 生成後のファイルを見比べて、差分が意図どおりモジュール定義だけに収まっているか確認する

## make / Python スクリプトへの接続方針

実装時は、Python スクリプトが `*.template` を走査して必要なファイルを生成し、その後に `run_test_matrix.py` を呼ぶ構成を想定する。

流れは次のイメージ。

```text
make
  -> template generator.py
  -> run_test_matrix.py
```

責務分担は以下を想定する。

- template generator
  - `*.template` の解釈
  - 必要なバリアントのファイル生成
  - 必要なら生成ファイルの clean
- `run_test_matrix.py`
  - 生成済みソースを使った compare / transform 実行
  - template ごとの compare 対象バリアントの解釈

## 初回導入候補

最初の適用対象としては、次の条件を満たす既存テストが向いている。

- HSP と chsp で本体ロジックがほぼ同じ
- `target=c` と plugin の差が `#chsp_module` 宣言中心に収まる
- 出力比較だけで十分に妥当性を見られる

たとえば、算術系や三角関数系の小さなテストは導入しやすい。
