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

### label 型引数と gosub によるコールバック呼び出し (`target=plugin`)

`target=plugin` では、仮引数として HSP のラベルを受け取る `label` 型に対応しています。
ディスパッチ関数（`cmdfunc` / `reffunc`）では `code_getlb()` を用いてラベルポインタ（`unsigned short *`）を取得します。

関数内からは `gosub <label>` 命令により、`code_call(label)` を介して HSP 側のサブルーチンを直接コールバック呼び出しできます。

#### 終了状態 (RUNMODE_END) の伝搬ガード

HSP 側のサブルーチンが `end` 命令等によって終了した場合、ランタイム状態が `ctx->runmode == RUNMODE_END` に遷移します。
このとき、コールバック呼び出し元のネイティブ関数が後続処理や副作用を継続して実行しないよう、以下の位置に終了伝搬ガード（`if ( ctx->runmode == RUNMODE_END ) return;`）を自動生成します。

- ネイティブ関数の先頭
- ネイティブ関数・コールバック呼び出し文の直後
- 連続代入における各代入評価の直後
- `dim` / `dimtype` 命令の引数式評価後
- `if` 条件式や `repeat` 回数式にネイティブ呼び出しが含まれる場合（評価結果を一時変数へ退避し、ガード実行後に条件判定やループへ進行）
- プラグインディスパッチ関数:
  - `cmdfunc`: `return ctx->runmode` を返却
  - `reffunc`: `ctx->runmode == RUNMODE_END` を検知した場合、`puterror(HSPERR_NONE)` を呼び出して後続の引数パーシングを行わずに即時復帰

### 64bit / int64_t 対応と ABI 定義

64bit 環境でのプラグイン ABI 整合性を確保するため、`common/chsp/hsp3struct.h` を更新しています。

- `HSPINT64` (`ptrdiff_t`), `HSPPTRINT` (64bit時: `ptrdiff_t`, 32bit時: `int`), `HSPCTX_STAT_FLAG` の定義を追加
- `HSPCTX` の `stat`, `strsize`, `iparam` 等のフィールド型を `HSPPTRINT` に更新
- `IRQDAT` コールバックや `HSPEXINFO30` / `HSPEXINFO` / `HSP3TYPEINFO` の型をポインタサイズに対応

#### HSP64 定義制御

プラグイン生成時の C ソース preamble および `libtcc` シンボル定義において、アーキテクチャやポインタ幅（`__x86_64__`, `__UINTPTR_MAX__` 等）からの自動推論による `HSP64` 強制定義を廃止しました。
ホストコンパイラ/ランタイムのビルド時に明示的に設定された `HSP64` 定義状態を引き継ぐ設計となっており、非 64bit ビルド環境での誤定義を防止しています。

### 関数シグネチャのパース（後置アロー記法）

`#cfunc ... -> <type>` との記法整合を図るため、`#chsp_defcfunc` の構文を `#chsp_defcfunc <name> <params...> -> <rettype>` に統一しています。
- 引数リスト末尾の `-> <rettype>` をパーサーで検証
- 旧来の前置型指定はコンパイルエラーとして拒絶
- `#chsp_deffunc` においては、末尾の `-> void` 指定を許容

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
呼び出し側 (`chsp_frontend.cpp`) はエラー時にコンパイルエラーとして処理します。

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

### 対応外の機能・制限事項

以下は現状の実装に含まれません。将来の拡張候補です。

- `str` / `array[str]`
- cHSP ブロック内の `ddim` / `sdim`
- cHSP ブロックから通常の HSP 関数や標準命令を呼ぶこと（ただし `target=plugin` では `label` 引数を受け取り `code_call` による `gosub` コールバック呼出に対応）
- cHSP ブロック内部での HSP プリプロセッサのマクロ展開
- `gettime` の cHSP ブロック内利用
- HSP の一般的な「任意位置の引数省略」
- `rnd` / `randomize` の HSP ランタイムとの乱数状態共有

#### cHSP からの HSP 関数・標準命令呼び出し制限の理由

HSP3 の命令 (`cmdfunc`) や関数は引数スタックを持たず、実行中のバイトコードストリーム (`mcs`) から `code_get` / `code_next` を介して式を逐次評価・消費する構造になっています。そのため、C ネイティブ側から引数を渡して直接呼ぶ手段が存在しません。擬似バイトコード生成やサブルーチンブリッジ (`HspFunc_call`) などの回避策も大きなオーバーヘッドや状態破壊のリスクを伴うため、非対応としています（詳細は [decisions/chsp-plugin-call-investigation.md](decisions/chsp-plugin-call-investigation.md) を参照。なお、引数を持たないサブルーチンへのコールバック呼び出しについては `target=plugin` の `gosub <label>` として実装済みです）。

### 将来の拡張方針

- hsp 側とネイティブ側双方にラッパー処理を生成することで対応文法を拡大できる可能性がある
- グローバル変数参照を検出して自動で引数に変換する (変数の型推論が課題)
- `str` の返り値対応
