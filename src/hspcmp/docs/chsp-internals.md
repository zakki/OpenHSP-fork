# cHSP コンパイラ内部設計

cHSP コンパイラ実装者向けのドキュメントです。
ユーザー向け仕様は [chsp.md](chsp.md) を参照してください。

## 処理フロー

1. **`.chsp` ファイルのパース**:
   - `#chsp_*` ブロックとそれ以外の HSP コードを分離します。
   - MVP ではこの段階を既存 HSP プリプロセッサより前に実行します。
2. **ネイティブコード生成**:
   - `#chsp_*` ブロック内のコードを C の関数に変換します。
   - 各 `#chsp_module` の target 指定に応じて、plugin backend の補助関数呼び出しか、素の C backend 向けコードに変換します。
3. **HSP コード生成**:
   - `#chsp_*` ブロックを、生成したネイティブ関数を呼び出す `#uselib`、`#func`、`#cfunc` 命令に置き換えます。
4. **ファイル出力**:
   - `.ax` とネイティブソース (`.c`) を出力します。
5. **ネイティブコードのコンパイル**:
   - 生成された `.c` を、DLL や共有ライブラリにコンパイルします。
   - コンパイルされたライブラリは、生成された `.hsp` / `.ax` から呼び出されます。

## 2 系統の backend

cHSP は以下の 2 つの backend を持ちます。

### plain backend (`target=c`)

- HSP 側: `#uselib` + `#func` / `#cfunc`
- ネイティブ側: 通常の C ABI でエクスポートされた関数
- 用途: HSP ランタイムへの依存がない計算専用コード

### plugin backend (`target=plugin`, 既定)

- HSP 側: `hsp3cmdinit` + plugin 命令定義
- ネイティブ側: HSP plugin ABI (cmdfunc / reffunc)
- 用途: `HSPEXINFO` 経由の HSP ランタイム機能 (配列アクセス、変数参照等) が必要なコード

plugin backend の詳細な設計経緯は [decisions/chsp-plugin-backend.md](decisions/chsp-plugin-backend.md) を参照してください。

### plugin backend の構造

plugin backend では生成コードの最小形は以下です。

```c
EXPORT void WINAPI hsp3cmdinit(HSP3TYPEINFO *info);
static int cmdfunc(int cmd);
static void *reffunc(int *type_res, int cmd);
```

- `#chsp_deffunc` → `cmdfunc(cmd)` のディスパッチ先
- `#chsp_defcfunc` → `reffunc(type_res, cmd)` のディスパッチ先
- `cmd` は cHSP 関数ごとの連番 ID

### `HSPEXINFO` で使用できる操作

`hsp3cmdinit(HSP3TYPEINFO *info)` が呼ばれ、`info->hspexinfo` から以下の操作を取得します。

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

## `chsp_builtins.tsv` の形式とロード

組み込み関数マッピングは `{compath}/chsp/chsp_builtins.tsv` に置かれます (`common/chsp/chsp_builtins.tsv`)。

### フォーマット

タブ区切りテキスト (TSV)。1 行 1 エントリ。

```
hsp_name\tc_target\tcpp_target\t[min_args\t[max_args]]
```

| カラム | 内容 |
| --- | --- |
| `hsp_name` | HSP 側の関数名 (小文字) |
| `c_target` | `target=c` 時の出力名 |
| `cpp_target` | `target=plugin` 時の出力名 |
| `min_args` | 最小引数数 (-1 = 無制限、省略時 -1) |
| `max_args` | 最大引数数 (-1 = 無制限、省略時 -1) |

`#` で始まる行はコメント。空行は無視。

引数数によって変換先が変わる場合は複数行で定義します。例:

```tsv
randomize	chsp_randomize	chsp_randomize	0	0
randomize	chsp_randomize_seed	chsp_randomize_seed	1	-1
```

### ロード

`LoadChspBuiltinMap(compath, out)` (`chsp_builtin_map.cpp`) が `{compath}/chsp/chsp_builtins.tsv` を読み込みます。
ファイルが見つからない・読めない場合はエラー文字列を返します (空文字列 = 成功)。
呼び出し側 (`chsp_frontend_v2.cpp`) はエラー時にコンパイルエラーとして処理します。

## 識別子の正規化と C 関数名

HSP の識別子は大文字小文字を区別しません。`chsputil::NormalizeIdentifier()` で小文字に統一します。

ただし C は大文字小文字を区別するため、`#chsp_cdecl` で宣言した C 関数名は正規化してはいけません。
`declared_native_functions` は `map<normalized_name, original_case_name>` で管理し、出力時は original case を使います。

## 回帰テスト

`test/test_chsp_compare/Makefile` に各種テストターゲットがあります。

```sh
make -C test/test_chsp_compare check-emit-c
make -C test/test_chsp_compare check-emit-c-tcc
make -C test/test_chsp_compare check-libtcc
```

- `check-emit-c`: C backend を `cc` で共有ライブラリ化して比較テスト・transform テストを通します
- `check-emit-c-tcc`: C backend を `tcc` で共有ライブラリ化して同テストを通します
- `check-libtcc`: `hspcmp --chsp-compile=libtcc` で直接共有ライブラリを出力し比較テストを通します

mixed target 対応後は「plugin module のみ」「C module のみ」「plugin / C 混在」の 3 パターンを比較テストで通します。

## デバッグ出力

コンパイラ開発者向けのデバッグ出力は環境変数 `CHSP_DEBUG=1` で有効になります。
AST の JSON ダンプや入力テキストのエコーが出力されます。

HSP スクリプト作者向けのデバッグ (`cg_debug()` / `COMP_MODE_DEBUG`) とは別物です。

## HSP ランタイム API の将来計画

MVP では HSP SDK 連携は行わず、純粋な C ABI で受け渡し可能な型だけを対象にします。
将来的に HSP ランタイム連携を導入する場合は、`ddim` / `sdim`、文字列、HSP 関数呼び出しなどをこの層で扱います。

plugin backend は `HSPEXINFO` 経由でこれらの多くをすでに扱えるため、
将来の拡張では plain backend に独自 runtime bridge を追加するより plugin backend の活用を先に検討すること。

## 既知の実装上の課題

- `int` / `double` の混在演算と、混在式を `int` / `double` 返り値や代入先へ載せたときのセマンティクスは未確定
  - HSP 準拠に寄せるか C の usual arithmetic conversions に寄せるかは今後の検討事項
- 現在は compare テストで plugin backend の現挙動を観測・固定している段階

### MVP では対応外の機能

以下は現状の実装に含まれません。将来の拡張候補です。

- `str` / `array[str]`
- cHSP ブロック内の `ddim` / `sdim`
- cHSP ブロックから通常の HSP 関数を呼ぶこと
- cHSP ブロック内部での HSP プリプロセッサのマクロ展開
- `gettime` の cHSP ブロック内利用
- HSP の一般的な「任意位置の引数省略」
- `rnd` / `randomize` の HSP ランタイムとの乱数状態共有

### 将来の拡張方針

- hsp 側とネイティブ側双方にラッパー処理を生成することで対応文法を拡大できる可能性がある
- グローバル変数参照を検出して自動で引数に変換する (変数の型推論が課題)
- `double` / `str` の返り値対応

## 他の手法との比較

### hsp3cnv

- Android / iOS ターゲットの hsp3dish 開発で使われる HSP → C++ コンバーター
- HSP 同様に型は緩く、HSP の文法をほぼそのまま使える
- 速度上のメリットはほぼない

### hsp3ll

- HSP の文法をそのまま使える LLVM ベースの JIT コンパイラ
- 実行時の型情報を利用して型を推論し最適化する
- JIT の実装コストが高く未完成。最適化のために各種 HSP 命令を型ごとに特殊化した LLVM IR ライブラリが必要

### cHSP の位置づけ

- HSP の文法を拡張して C コードに変換する
- 変数の型を明示的に指定する必要がある (C の型システムをそのまま活用)
- 既存 HSP スクリプトを部分的にネイティブ化できる
- コンパイルエラーや実行時エラーのデバッグには C の知識が必要
