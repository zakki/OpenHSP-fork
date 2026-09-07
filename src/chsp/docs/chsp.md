# cHSP: HSPスクリプト高速化のための拡張

## 概要

cHSPは、HSPスクリプトの一部をネイティブコードに変換し、コンパイルすることで、実行速度を向上させるための仕組みです。

## 仕様

### cHSPファイル (`.chsp` もしくは `.hsp`)

- HSPスクリプトの文法を基本とします。
- 高速化したい関数を含むモジュールを `#chsp_module` と `#chsp_module_end` で囲みます。
  - module 単位で native target を指定できます。
- 高速化したい関数は`#chsp_defcfunc`や`#chsp_deffunc`として定義します。
  - この関数は `#chsp_end` で終了します。
- module 内では `#chsp_c {"..."}` でネイティブ側へそのまま埋め込む C コードを書けます。
- module 内では `#chsp_cdecl name` で、`#chsp_c` 内に書いた C 関数名を cHSP から呼べるようにできます。
- module 内では `#chsp_clink "name"` で、その module の共有ライブラリ生成時に追加でリンクするライブラリを指定できます。
- cHSP 関数の引数やローカル変数には型指定が必須です。
- cHSP ブロック内のローカル変数・ローカル配列は `local[...]` で宣言します。

### 対応する型と文法

- 型
  - `int`
  - `double`
  - `array[int]`
  - `array[double]`
  - `local[int]`
  - `local[double]`
  - `local[int[n]]`
  - `local[double[n]]`
  - `label` (`target=plugin` のみ。HSP側のラベルを引数として受け取り、コールバック呼び出しに利用可能)
- 関数
  - `#chsp_deffunc`
  - `#chsp_defcfunc`
- 文
  - 代入 (`+=`, `-=`, `*=`, `/=` の複合代入を含む)
  - `if` / `else if` / `else`
  - `if 条件 : 文` の1行形式
  - `repeat` / `loop` / `continue` / `break`
  - `gosub <label>` (`target=plugin` のみ。渡された `label` 型引数をコールバックとしてサブルーチン呼び出し)
  - `return`
- 式
  - 算術演算
  - 比較演算
  - ビット演算
  - シフト演算
  - 組み込み関数呼び出し
  - 数値配列アクセス
    - 引数配列は `a(i)`, `a(i,j)`, `a(i,j,k)`, `a(i,j,k,l)` に対応
    - `local[int[n]]` / `local[double[n]]` は従来どおり 1 次元固定長配列のみ対応

注意:

- `int` / `double` の混在演算や、混在式を `int` / `double` の返り値・代入先へ渡す場合の暗黙変換セマンティクスは仕様策定中です（現状は代入先や型に応じた C の型変換規則に準拠）。

### 現在非対応の機能・制約事項

- `str` / `array[str]`
- cHSP ブロック内の `ddim` / `sdim`
- cHSP ブロックから通常の HSP 関数を直接呼ぶこと（ただし `target=plugin` では `label` 引数を介した `gosub` コールバックが可能）
- cHSP ブロック内部での HSP プリプロセッサのマクロ展開
- `gettime` の cHSP ブロック内利用
- HSP の一般的な「任意位置の引数省略」

### include の制限と分割ビルド

現在、cHSP の変換対象は入口ファイルに直接記述された `#chsp_*` ブロックだけです。`#include "kernel.chsp"` と書いても、include 先の定義はネイティブ化されません。拡張子を `.hsp` に変更したり、入口に空の `#chsp_module` を追加したりしても解決しません。CLI と Proxy DLL の両方にこの制限があります。

cHSP の変換後に通常の hspcmp が include を処理するため、未変換の定義が渡ってもコンパイルが成功する場合があります。未初期化変数の警告、関数呼び出しのコンパイルエラー、実行時の Error 10（サブルーチン外の return）などにつながります。終了コード 0 だけではネイティブ化の成功を確認できません。

入口ファイルに定義を直接記述するか、`--library` で include 用ライブラリを先に生成してください。

1. `chsp --library --compath=<commonのパス>/ kernel.chsp` を実行します。
2. 通常の HSP 側で `#include "kernel.as"` と記述し、通常の hspcmp に `-i` を指定して利用側をビルドします。
3. module ごとに生成される共有ライブラリを実行時に読み込める場所に配置します。元の `.chsp` を変更したら、ライブラリと利用側を再ビルドしてください。

ライブラリモードでは `.ax` や `.chsp.tmp.hsp` を生成せず、hspcmp へも委譲しません。module 外の HSP コードは順序を保って `.as` に残り、利用側で include した位置に従って実行されます。include ガードは重複挿入を防ぎますが、実行時の初期化回数を制御するものではありません。

`-o<出力先>.as` でヘッダーの出力先を指定でき、ネイティブ出力も同じディレクトリに配置します。入力中の相対パスは書き換えません。具体例と配置規則は [ライブラリ生成の手順](../README.md#include-用ライブラリの生成) を参照してください。

従来の `--keep-tmp` で保持した `.chsp.tmp.hsp` を include する方法も使えますが、正式な配布・利用には include ガード付きの `.as` を生成する `--library` を使用してください。

include の再帰展開、条件付き include・ガードの評価、include 先の元ファイル・行番号を維持した cHSP 変換は未対応です。通常の HSP コードに対する include 処理は引き続き hspcmp が行います。Windows CLI の必要ファイルと具体例は [README](../README.md#windows-cli-の必要ファイルと実行例) を参照してください。

### 生成されるファイル

`--library` モードでは `.ax` の代わりに include 用の `.as` を生成します。`.as` には登録定義と module 外の HSP コードが入り、include ガードが付きます。

通常モードの CLI では、`--keep-tmp` は通常削除される `<入力名>.chsp.tmp.hsp` を保持します。`.c` と共有ライブラリの保持には不要です。`--chsp-compile=none` では共有ライブラリのビルドを省略します。通常モードでは HSP コンパイルへの委譲を行い、`--library` モードでは行いません。

- **最適化AXスクリプト (`.ax`)**:
  - `#chsp_*`ブロックが、ネイティブコードで実装された機能を呼び出すHSPコードに置き換えられます。
  - ネイティブ側で処理される変数の受け渡し処理などが自動的に挿入されます。
- **Cソースコード (`.c`)**:
  - module ごとに target に応じた C ソースを生成します。
  - target 未指定の module は既定で plugin backend 用の C ソースを生成します。
- **共有ライブラリ (`.so` / `.dll`)**:
  - ネイティブコンパイル方式（既定: `libtcc`）により、module ごとに共有ライブラリがビルドされます。

### 変数共有

- 引数として HSP の数値と数値配列をネイティブ側に渡します。
- `target=plugin` では、引数として `label` 型（HSPのラベルポインタ）を渡すことができます。
- HSP側のグローバル変数はネイティブ側では利用できません。
- cHSP ブロックから通常の HSP 関数は直接呼べませんが、`target=plugin` では `label` 引数を介した `gosub` コールバックが利用可能です。

## 設計

### コンパイラフロントエンド (`chsp` / `hspcmp.dll`)

Linux では `chsp` CLI、Windows では `chsp.exe` CLI または `hspcmp.dll` (Proxy DLL) が `.chsp` ファイルを解釈し、ネイティブソース (`.c`) や共有ライブラリ (`.so` / `.dll`) を生成したうえで、`hspcmp` へ委譲して `.ax` を生成します。
拡張構文を含まない通常の `.hsp` ファイルを受け取った場合は、既存の `hspcmp` と同様の処理をそのまま実行します。

コンパイラの内部構造については [chsp-internals.md](chsp-internals.md) を参照してください。

#### `#chsp_module` ごとの native target 指定

- backend の既定値は `plugin`
- 既定の native compile mode は `libtcc`
- backend の指定は CLI オプションではなく、`#chsp_module` 行で行います
- compile mode の指定は従来どおり `--chsp-compile=...` で行います

書式:

```hsp
#chsp_module "ao_opt" target=plugin
    ; ...
#chsp_module_end

#chsp_module target=c
    ; ...
#chsp_module_end
```

- 先頭の module 名文字列は省略できます。
  - 省略時はソースファイル名と module の出現順から出力ファイル名を自動生成します。
- `target=plugin`
  - plugin backend を使います。
- `target=c`
  - 素の C backend を使います。
- target を省略した場合
  - `target=plugin` と同じ扱いにします。

`target=c` (C backend) では、`rnd` / `randomize` は標準 C の `rand` / `srand` ベースで動作します。`mt19937` (`HSPRANDMT`) には対応しません。

現在の既定動作は、target 未指定 module を plugin backend として扱い、`--chsp-compile=libtcc` と組み合わせてその場で共有ライブラリまで出力する形です。

`--chsp-compile=libtcc` は Linux と Win32 で使えます。混在した target を含む `.chsp` でも、各 module から生成した `.c` を順に `libtcc` に渡して共有ライブラリまで出力します。

#### module 内に補助 C コードを書く

`#chsp_c {"..."}` を使うと、module ごとのネイティブソースへ C コードをそのまま埋め込めます。

```hsp
#chsp_module "native_helper_sample" target=c

#chsp_c {"
static int helper(int v) {
    return v + 1;
}
"}
#chsp_cdecl helper

#chsp_defcfunc add_one int v -> int
    return helper(v)
#chsp_end

#chsp_module_end
```

- `#chsp_c {"..."}` は module の外側では使えません。
- `#chsp_cdecl` も module の外側や cHSP 関数本体の中では使えません。
- `#chsp_cdecl` した名前だけを cHSP から呼べます。
- `#chsp_c` に書いた C コードは、`target=c` / `target=plugin` のどちらでも使えます。

#### module ごとに追加ライブラリをリンクする

ネイティブコードが標準の既定リンク以外のライブラリを必要とする場合は、`#chsp_clink "name"` を使います。

```hsp
#chsp_module "dl_sample" target=c

#chsp_c {"
#include <dlfcn.h>

static int can_open_self(void) {
    void *handle = dlopen(NULL, RTLD_LAZY);
    if (handle == NULL) {
        return 0;
    }
    dlclose(handle);
    return 1;
}
"}
#chsp_cdecl can_open_self
#chsp_clink "dl"

#chsp_defcfunc check_dl -> int
    return can_open_self()
#chsp_end

#chsp_module_end
```

- `#chsp_clink` は module ごとに有効です。
- 1 行に 1 つのライブラリ名を書きます。複数必要な場合は複数行書きます。
- Linux では `#chsp_clink "dl"` のように、通常の `-l<name>` に相当する `<name>` 部分だけを書きます。
- `#chsp_clink` は `target=c` / `target=plugin` のどちらでも使えます。

#### label 型引数と gosub コールバック (`target=plugin`)

`target=plugin` では、HSP のラベルを関数の引数として受け取り、cHSP 側から `gosub` 命令を実行して HSP 側のサブルーチンをコールバック呼び出しできます。

```hsp
#chsp_module "callback_demo" target=plugin

#chsp_deffunc iterate_with_callback int n, label cb
    repeat n
        gosub cb
        if stat != 0 {
            break
        }
    loop
    return
#chsp_end

#chsp_module_end

// HSP側スクリプト
iterate_with_callback 5, *on_step
stop

*on_step
    mes "Callback called: cnt=" + cnt
    return 0
```

- **型と制約**:
  - `label` 型引数および `gosub` 命令は `target=plugin` 専用です（`target=c` ではコンパイルエラーになります）。
  - `gosub` の引数には、関数の引数リストで宣言された `label` 型変数のみ指定できます（ローカル変数、配列、即値ラベルは不可）。
- **戻り値と制御**:
  - HSP サブルーチン側で `return <値>` を実行すると、その戻り値が `stat`（`ctx->stat`）に格納されるため、cHSP スクリプト内から `stat` を直接参照してループの中断や条件分岐に利用できます。
  - HSP サブルーチン内で `end` 等により実行が終了した場合、ランタイム終了状態（`ctx->runmode == RUNMODE_END`）を検知して cHSP 側も安全に関数を終了します。

#### mixed target を含む `.chsp` の出力方針

- `#chsp_module` ごとに独立した `.c` と共有ライブラリを生成します。
- 生成される HSP 側コードでは、module ごとに対応する `#uselib` を 1 回だけ出力します。
- 同一 `.chsp` 内で `target=plugin` と `target=c` を混在させられます。
- 出力ファイル名は module 単位で一意になる必要があります。
  - module 名省略時はモジュールの出現順を使い、`foo.chsp` から `foo_1.c` / `foo_1.so`、`foo_2.c` / `foo_2.so` のように出力します。

```sh
./chsp -d -i -u --compath=common/ src/chsp/sample/ao_opt.chsp
```

Linux では `src/chsp/sample/ao_opt.c` と `src/chsp/sample/ao_opt.so`、Win32 では `src\chsp\sample\ao_opt.c` と `src\chsp\sample\ao_opt.dll` がまとめて生成されます。

Win32 では `libtcc` のヘッダと import library を参照できるように、`src/chsp/win32/chsp.vcxproj` や `src/chsp/win32dll/hspcmp.vcxproj` が `src/chsp/extlib/tcc/libtcc` を見に行きます。実行時の `libtcc` ランタイム探索は以下の順です。

- `LIBTCC_DIR`
- 実行ファイルと同じディレクトリの `tcc\`

### 生成された `.c` のビルド方法

生成された `.c` は、ビルド時には OpenHSP リポジトリのルートを include path に含めます。
Windows 向けの生成コードは `CHSP_EXPORT` マクロで `__declspec(dllexport)` が付くため、追加の `.def` は不要です。

以下では、リポジトリのルートで `src/chsp/sample/ao_opt.chsp` から `src/chsp/sample/ao_opt.c` を生成済みとします。

#### Linux

`.so` を生成します。

```sh
cc -std=c11 -O2 -shared -fPIC -I. -o src/chsp/sample/ao_opt.so src/chsp/sample/ao_opt.c -lm
```

`#chsp_clink` を使った module は、必要なライブラリを追加してビルドします。たとえば `#chsp_clink "dl"` を使っている場合は次のようになります。

```sh
cc -std=c11 -O2 -shared -fPIC -I. -o src/chsp/sample/dl_sample.so src/chsp/sample/dl_sample.c -lm -ldl
```

HSP 側の `#uselib` は Linux では `.so` を参照します。

```hsp
#uselib "ao_opt.so"
```

#### Win32

Visual Studio の `x86 Native Tools Command Prompt for VS` など、32bit 向けの MSVC 環境を開いてから `cl` を実行します。

```bat
cl /O2 /LD /I. /Fe:src\chsp\sample\ao_opt.dll src\chsp\sample\ao_opt.c
```

追加ライブラリが必要な場合は、`#chsp_clink` に対応する import library を `cl` の引数へ追加します。

HSP 側の `#uselib` は `.dll` を参照します。

```hsp
#uselib "ao_opt.dll"
```

#### Win64

Visual Studio の `x64 Native Tools Command Prompt for VS` など、64bit 向けの MSVC 環境を開いてから `cl` を実行します。

```bat
cl /O2 /LD /I. /Fe:src\chsp\sample\ao_opt.dll src\chsp\sample\ao_opt.c
```

出力ファイル名は Win32 と同じ `.dll` で問題ありません。32bit 用 HSP からは Win32 版 DLL、64bit 用 HSP からは Win64 版 DLL を読み込ませます。

#### 追加メモ

- `rnd` / `randomize` の挙動を HSP ランタイムと合わせたい場合は、必要に応じて `HSPRANDMT` を定義してビルドします。
- デバッグビルドにしたい場合は、Linux では `-g`、MSVC では `/Zi` を追加します。
- 生成された `.hsp` / `.ax` の `#uselib` に書かれたファイル名と、実際に生成した共有ライブラリのファイル名を一致させてください。

### 組み込み関数

組み込み関数のマッピングは `common/chsp/chsp_builtins.tsv` で定義されています。
HSP 名をそのまま受け付け、ネイティブ側では plugin backend の補助関数または C backend 向けの実装へ変換します。

対応済みの主な関数:

- `int`
- `double`
- `length`
- `length2`
- `length3`
- `length4`
- `abs`
- `absf`
- `sin`
- `cos`
- `tan`
- `atan`
- `sqrt`
- `expf`
- `logf`
- `powf`
- `limit`
- `limitf`
- `rnd`
- `randomize`

`rnd` / `randomize` は cHSP 側の独立実装です。乱数状態は HSP ランタイムと共有しません。
ただし、`HSPRANDMT` を定義してビルドした場合は Mersenne Twister 分岐、未定義の場合は `rand()` 分岐になり、`src/hsp3/hsp3int.cpp` の条件分岐に合わせられます。

## aobenchの例

`aobench`は、アンビエントオクルージョンという3DCGのレンダリング手法のベンチマークプログラムです。
このサンプル（`src/chsp/sample/` 配下に収録）では、`ao_original.hsp`（オリジナルのHSPスクリプト）の処理のうち、特に計算負荷の高いレイトレーシングの部分を`#chsp`ブロックに記述し、ネイティブコードに置き換えることで高速化を図っています (`ao_opt.chsp`)。

このように、cHSPは計算量の多い処理をネイティブコードにオフロードすることで、HSPスクリプトの実行速度を6倍程度に向上させることができる例を示しています。

### `ao_original.hsp` と `ao_opt.chsp` の主な変更点

`ao_opt.chsp`では、パフォーマンス向上のため、以下の関数が`#chsp`ブロックで囲われ、ネイティブコードとしてコンパイルされるように変更されています。

- ベクトル計算: `vdot`, `vcross`, `vnormalize` などのベクトル演算関数。
- レイとオブジェクトの交差判定: `ray_sphere_intersect`, `ray_plane_intersect` といった、レイトレーシングの中核となる関数。

これらの関数は、ピクセルごとに何度も呼び出されるため、ネイティブ化による高速化の効果が特に大きくなります。

DLLに分離したネイティブ関数として実装するため変更が必要です。

- 多次元配列を1次元配列に書き換える
- グローバル変数参照を関数の引数に書き換える

ソースコード: `src/chsp/sample/ao_opt.chsp`

```hsp
#chsp_deffunc vcross array[double] c, array[double] v0, array[double] v1
    c(0) = v0(1) * v1(2) - v0(2) * v1(1)
    c(1) = v0(2) * v1(0) - v0(0) * v1(2)
    c(2) = v0(0) * v1(1) - v0(1) * v1(0)
    return
#chsp_end
```

生成されるCコードのイメージ (`target=c` の場合):

```c
CHSP_EXPORT void vcross(double *c, double *v0, double *v1) {
    c[0] = v0[1] * v1[2] - v0[2] * v1[1];
    c[1] = v0[2] * v1[0] - v0[0] * v1[2];
    c[2] = v0[0] * v1[1] - v0[1] * v1[0];
    return;
}
```

※ 既定の `target=plugin` では、直接関数をエクスポートする代わりに HSP3 プラグインエントリーポイント（`hsp3cmdinit`, `cmdfunc`, `reffunc`）を介して安全・高速に呼び出される C コードが生成されます。

生成されるAXコードと等価なHSPコード (`target=c` の場合):

```hsp
#uselib "ao_opt.so"
// Windows の場合は #uselib "ao_opt.dll"
#func global vcross "vcross" var, var, var
```

