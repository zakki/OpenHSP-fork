# cHSP plugin backend 案

## 概要

別案として、`src/plugins/win32/hpi3sample/` のような HSP plugin DLL ABI を cHSP の出力先として使う方法があります。
この方式では、独自 `ChspHspRuntime` bridge を作らず、HSP が既にプラグインへ渡している `HSP3TYPEINFO` / `HSPEXINFO` をそのまま利用します。

## 既存 ABI で使えるもの

`hpi3sample` では `hsp3cmdinit( HSP3TYPEINFO *info )` が呼ばれ、`info->hspexinfo` から以下の操作を取得しています。

- 引数走査
  - `HspFunc_prm_next`
  - `HspFunc_prm_geti`
  - `HspFunc_prm_getdi`
  - `HspFunc_prm_gets`
  - `HspFunc_prm_get`
  - `HspFunc_prm_getva`
  - `HspFunc_prm_setva`
- ランタイム呼び出し
  - `HspFunc_call`
  - `HspFunc_puterror`
  - `HspFunc_hspevent`
- 変数/配列
  - `HspFunc_getproc`
  - `HspFunc_dim`
  - `HspFunc_redim`
  - `HspFunc_array`
- 補助
  - `HspFunc_malloc`
  - `HspFunc_free`
  - `HspFunc_expand`

つまり、cHSP が必要としている「HSP 命令/関数の引数取得」「PVal 経由の配列アクセス」「HSP 関数呼び出し」の大部分は、既存 plugin ABI ですでに賄えます。

## 生成形態

この案では、cHSP モジュールを通常の `#func` / `#cfunc` 用 DLL ではなく、HSP plugin DLL として生成します。
ネイティブ側の最小形は以下です。

```c
EXPORT void WINAPI hsp3cmdinit(HSP3TYPEINFO *info);
static int cmdfunc(int cmd);
static void *reffunc(int *type_res, int cmd);
```

- `#chsp_deffunc` は plugin の `cmdfunc(cmd)` ディスパッチ先に変換する
- `#chsp_defcfunc` は plugin の `reffunc(type_res, cmd)` ディスパッチ先に変換する
- `cmd` は cHSP 関数ごとの連番 ID とする

生成される HSP 側コードも `#func` / `#cfunc` ではなく、プラグイン命令定義に合わせた形へ変わります。
つまりこの案は「plain mode の延長」ではなく、「出力 backend を plugin backend に切り替える」設計です。

## 利点

- 独自 bridge ABI を新設しなくてよい
- `HSPEXINFO` により、`code_getva` / `code_setva` / `code_call` を既存流儀のまま使える
- `PVal` と `HspVarProc` の解決責務を HSP plugin SDK 側に寄せられる
- `newcmd2` サンプルのように、変数引数を直接受けて代入する命令へ落としやすい
- `reffunc` を使えば `#chsp_defcfunc` の整数/実数返却にも自然に対応できる

## 制約

- HSP plugin は HSP の「命令/関数」として登録される仕組みであり、通常 DLL の `#func` / `#cfunc` とはロード経路が異なる
- 既存の cHSP 実装は `#uselib` と `#func` / `#cfunc` を出す前提なので、HSP 側 emitter を別分岐にする必要がある
- plugin ABI は HSP ランタイム内部 API への依存が強く、plain mode より移植性が低い
- `hpi3sample` は Win32 向けの SDK サンプルで、Linux など他ターゲットでは同じ配布形態か確認が必要
- plugin 1 本で複数の cHSP 関数を束ねる設計になるため、関数単位の独立 shared library より生成物粒度が粗くなる

## cHSP への適用イメージ

plugin backend では、cHSP の各関数を「HSP が直接呼ぶ命令/関数」に変換します。

- `#chsp_deffunc foo int a, array[int] b`
  - plugin `cmdfunc(CHSP_CMD_FOO)` に変換
  - 本体冒頭で `code_getdi` / `code_getva` を呼んで引数をデコード
- `#chsp_defcfunc bar double x`
  - plugin `reffunc(type_res, CHSP_FUNC_BAR)` に変換
  - 返値は plugin 側 static バッファか `ctx->refdval` / `refstr` ルールに合わせて返す

配列アクセスは `code_getva(&pval)` で `PVal*` と `APTR` を取り、必要に応じて `HspFunc_array`、`HspFunc_redim`、`HspFunc_getproc` を使って処理します。
この流れは、独自 runtime-linked mode を ABI で包むよりも、実装上は単純です。

## 問題になる点

plugin `cmdfunc` / `reffunc` は「HSP の現在のパラメータストリームを読む」モデルです。
そのため、cHSP の plain mode のような普通の C 関数呼び出し ABI にはなりません。

つまりこの案を採る場合、cHSP は 2 系統の backend を持つことになります。

- `plain backend`
  - `#uselib` + `#func` / `#cfunc`
  - C ABI の関数エクスポート
- `plugin backend`
  - `hsp3cmdinit` + `cmdfunc` / `reffunc`
  - HSP plugin ABI のエクスポート

両者はネイティブ関数のシグネチャも HSP 側のロード方法も別物なので、同一 DLL に無理に共存させるより backend として分離した方がよいです。

## 現時点の評価

HSP 命令呼び出し、配列操作、`code_call` による HSP 関数実行を行いたいのであれば、独自 runtime bridge 案より HSP plugin ABI の流用案の方が実装コストは低いです。
既存の `HSPEXINFO` がほぼ必要機能を持っているためです。

一方で、通常 DLL と同じ気軽さで `#func` / `#cfunc` 呼び出ししたい用途には向きません。
したがって仕様案としては次の住み分けが妥当です。

- 計算専用で HSP ランタイム非依存
  - 既存の plain backend を使う
- HSP 命令、配列再確保、ユーザー関数呼び出しが必要
  - plugin backend を追加し、HSP plugin ABI をそのまま使う

runtime-linked mode を実装するなら、独自 `ChspHspRuntime` を定義する前に、この plugin backend で要件を満たせるかを先に検証するのが順当です。
