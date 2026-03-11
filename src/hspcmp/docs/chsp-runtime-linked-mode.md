# cHSP runtime-linked mode 案

## 概要

純粋な `#func` / `#cfunc` ベースの DLL では、DLL 側から実行ファイル側の HSP ランタイム関数ポインタを取得できません。
そのため、cHSP で HSP 命令呼び出しや配列の `PVal` 操作を許可する場合は、純粋 C ABI とは別に runtime-linked mode を定義します。

- 既定の plain mode
  - 現行実装どおり `int` / `double` / `T*` のみをやり取りする
  - HSP ランタイム状態には触れない
- 追加する runtime-linked mode
  - HSP 実行ファイル側が cHSP DLL にランタイム API テーブルを渡す
  - cHSP DLL はその API テーブル経由で HSP 命令実行、配列操作、ユーザー関数呼び出しを行う

runtime-linked mode の初期化は、既存の `HSPEXINFO30` と `HspVarProc` をそのまま DLL に露出するのではなく、cHSP 用に必要部分だけを抜き出した安定 ABI にまとめます。
`HSPEXINFO30` は実装根拠として使いますが、生成 DLL が直接 `HSPEXINFO30` 全体に依存する形にはしません。

## 初期化シーケンス

`#func` / `#cfunc` だけでは関数ポインタ受け渡しができないため、runtime-linked mode ではホスト側の補助レイヤーが必要です。
想定する流れは以下です。

1. HSP ランタイム内蔵の bridge もしくは専用プラグインが `HSPEXINFO30` と `hspvarproc` から `ChspHspRuntime` を組み立てる
2. bridge が cHSP DLL をロードし、初期化エクスポート `chsp_set_runtime` を呼ぶ
3. cHSP DLL は受け取った `ChspHspRuntime` をモジュール内に保持する
4. 以降の cHSP 関数本体は、その API テーブル経由で HSP ランタイムを呼ぶ

想定する最小エクスポートは以下です。

```c
typedef struct ChspHspRuntime ChspHspRuntime;

int chsp_set_runtime(const ChspHspRuntime *runtime);
```

- `runtime == NULL` は未初期化状態に戻す
- version / size 不一致時は `0` を返して失敗とする
- 同一 DLL を複数の HSP コンテキストで共有しない前提を MVP とする
  - 将来的に複数コンテキストを許可する場合は、グローバル保持ではなく `ChspRuntimeContext *` を関数引数に渡す

## cHSP 用ランタイム ABI

`ChspHspRuntime` は `src/hsp3embed/hspvar_util.h`、`src/hsp3/hsp3struct.h`、`src/hsp3/hspvar_core.h` のうち cHSP が必要とする操作だけを公開します。

```c
typedef struct ChspPVal ChspPVal;
typedef int ChspAPtr;

typedef struct ChspVarProc {
    short flag;
    short aftertype;
    short version;
    unsigned short support;
    short basesize;
    short opt;
    void *(*GetPtr)(ChspPVal *pval);
    void (*Set)(ChspPVal *pval, void *pdat, const void *in);
    int (*GetSize)(const void *pdat);
    void *(*GetBlockSize)(ChspPVal *pval, void *pdat, int *size);
    void (*AllocBlock)(ChspPVal *pval, void *pdat, int size);
} ChspVarProc;

typedef struct ChspHspRuntime {
    int abi_version;
    int struct_size;

    void (*push_int)(int value);
    void (*push_double)(double value);
    void (*push_label)(int value);
    void (*push_str)(char *value);
    void (*push_var)(ChspPVal *pval, int aval);
    void (*push_vap)(ChspPVal *pval, int aval);
    void (*push_var_from_vap)(ChspPVal *pval, int aptr);
    void (*push_default)(void);
    void (*push_func_end)(void);

    void (*var_set)(ChspPVal *pval, int aval, int pnum);
    void (*var_calc)(ChspPVal *pval, int aval, int op);

    int (*hsp_if)(void);
    void (*extcmd)(int cmd, int pnum);
    void (*prgcmd)(int cmd, int pnum);
    void (*dllfunc)(int cmd, int pnum);
    void (*modcmd)(int cmd, int pnum);
    void (*usrfunc)(int cmd, int pnum);

    ChspVarProc *(*get_var_proc)(int flag);
    void *(*ptr_aptr)(ChspPVal *pval, ChspAPtr aptr);
    void (*dim)(ChspPVal *pval, int flag, int len0, int len1, int len2, int len3, int len4);
    void (*redim)(ChspPVal *pval, int lenid, int len);
    void (*array)(ChspPVal *pval, int offset);
    int (*count_elems)(ChspPVal *pval);

    void (*put_error)(int error);
    void (*call)(const unsigned short *pc);
} ChspHspRuntime;
```

ABI のポイント:

- スタック操作は `hspsource.cpp` / `hspvar_util.cpp` の `PushInt`、`PushVAP`、`PushFuncEnd`、`VarSet`、`Extcmd` などと 1 対 1 で対応させる
- 配列操作は `HspVarCoreDimFlex`、`HspVarCoreReDim`、`HspVarCoreArray`、`HspVarCorePtrAPTR` 相当を公開する
- `HspVarProc` 全量ではなく、cHSP が要素アクセスと再確保に必要な部分だけを `ChspVarProc` として固定する
- `call` は `code_call` 相当で、DLL 側から HSP ユーザー関数やラベルジャンプ先を起動するために使う

## 命令呼び出しの lowering

runtime-linked mode では、cHSP から HSP 命令を呼ぶときに `hspsource.cpp` と同じ「引数をスタックに積んでから実行関数を呼ぶ」形へ落とします。

例: `screen 0, x2, y2`

```c
runtime->push_vap(var_y2, 0);
runtime->push_vap(var_x2, 0);
runtime->push_int(0);
runtime->extcmd(42, 3);
```

例: `dim wall, wsx, wsy`

```c
runtime->push_vap(var_wsy, 0);
runtime->push_vap(var_wsx, 0);
runtime->push_vap(var_wall, 0);
runtime->prgcmd(9, 3);
```

この方式では、命令ごとの引数評価順、既定値処理、`if` 判定、`gosub` などを既存ランタイム実装に委譲できます。
cHSP 側は opcode 番号と必要な push 列を生成することに集中します。

## 配列と PVal の扱い

ランタイム連携を有効にした時点で、HSP 配列は単なる `T*` ではなく `PVal` ベースのハンドルとして扱う必要があります。
理由は以下です。

- `redim` や `dim` は再確保により先頭ポインタが変わる
- HSP の配列境界チェックと自動拡張は `PVal` の `len[]`、`offset`、`arraycnt` を使う
- `str` や将来のユーザー定義型は `basesize < 0` の可変長要素を取りうる

そのため runtime-linked mode の内部 IR では、配列引数を `ChspPVal*` として保持し、要素ポインタが必要な箇所だけ `ptr_aptr` と `get_var_proc` で解決します。

```c
ChspAPtr aptr = 0;
runtime->array(arr, index);
aptr = arr->offset;
double *ptr = (double *)runtime->ptr_aptr(arr, aptr);
```

生成コードではこの生 API をそのままばらまかず、`common/chsp/chsp_hsp_runtime.h` に薄いラッパーを置く想定です。

- `chsp_hsp_array_index_int(runtime, arr, index)`
- `chsp_hsp_array_index_double(runtime, arr, index)`
- `chsp_hsp_dim_int(runtime, arr, len1, len2, ...)`
- `chsp_hsp_redim(runtime, arr, lenid, len)`

`array[int]` / `array[double]` を plain mode と同じ記法で書けるようにしつつ、ランタイム連携が必要な場合だけ lowering を `T*` から `PVal*` ベースに切り替えます。

## DLL から HSP 関数を呼ぶ仕様

DLL 側から HSP のユーザー定義関数やラベルを呼ぶ場合は、ランタイムが保持している `pc` を引数として `call` に渡します。
この `pc` はコンパイル時に解決したラベル参照を bridge 経由で DLL に渡す必要があります。

- cHSP から通常の HSP 関数を呼ぶ構文を許可する場合、コンパイラは対象関数をラベル解決して `const unsigned short *pc` として保持する
- 実行時は `runtime->call(pc)` を呼ぶ
- 戻り値は HSP 標準の `stat` / `refstr` / `mpval` ルールに従うため、cHSP 側では別途「戻り値受け取りラッパー」を用意する

この点は plain mode の「通常 HSP 関数は呼べない」と明確に分離します。
MVP では runtime-linked mode でも `deffunc` 呼び出しのみを対象にし、`modfunc`、文字列戻り値、例外的な省略引数は後回しにします。

## 実装上の境界

- `PVal` / `HspVarProc` は HSP ランタイム内部構造なので、cHSP 側には `common/chsp/chsp_hsp_abi.h` で複製した最小定義だけを見せる
- `HSPEXINFO30` をそのまま公開すると HSP 本体更新時の ABI 破壊が大きいため、bridge 側で吸収する
- `code_call` は実行コンテキストに依存するため、bridge を経由せずに DLL が直接ホストシンボルを `dlsym` / `GetProcAddress` で探す設計は採らない
- plain mode と runtime-linked mode は同じ `.chsp` 文法を共有し、使用した機能に応じて必要モードを自動判定する

つまり cHSP の仕様としては、`int` / `double` だけで閉じる関数は従来どおり純粋 DLL として生成し、HSP 命令・配列再確保・HSP 関数呼び出しを含む関数だけを runtime-linked mode に昇格させるのが基本方針になります。
