# cHSP: HSPスクリプト高速化のための拡張

## 概要

cHSPは、HSPスクリプトの一部をネイティブコードに変換し、コンパイルすることで、実行速度を向上させるための仕組みです。

## 仕様

### cHSPファイル (`.chsp`)

- HSPスクリプトの文法を基本とします。
- 高速化したい関数を含むモジュールを `#chsp_module` と `#chsp_module_end` で囲みます。
  - module 単位で native target を指定できます。
- 高速化したい関数は`#chsp_defcfunc`や`#chsp_deffunc`として定義します。
  - この関数は `#chsp_end` で終了します。
- module 内では `#chsp_c {"..."}` でネイティブ側へそのまま埋め込む C コードを書けます。
- module 内では `#chsp_cdecl name` で、`#chsp_c` 内に書いた C 関数名を cHSP から呼べるようにできます。
- module 内では `#chsp_clink "name"` で、その module の共有ライブラリ生成時に追加でリンクするライブラリを指定できます。
- cHSP 関数の引数やローカル変数には型指定が必須です。
- MVP では、cHSP ブロック内のローカル変数・ローカル配列は `local[...]` で宣言します。

### MVP で対応する型と文法

- 型
  - `int`
  - `double`
  - `array[int]`
  - `array[double]`
  - `local[int]`
  - `local[double]`
  - `local[int[n]]`
  - `local[double[n]]`
- 関数
  - `#chsp_deffunc`
  - `#chsp_defcfunc`
- 文
  - 代入
  - 複合代入
  - `if` / `else if` / `else`
  - `if 条件 : 文` の1行形式
  - `repeat` / `loop`
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

- `int` / `double` の混在演算と、混在式を `int` / `double` 返り値や代入先へ載せたときのセマンティクスは未確定です。
- 現在は compare テストで plugin backend の現挙動を観測・固定している段階であり、HSP 準拠に寄せるか、C の usual arithmetic conversions に寄せるかは今後の検討事項です。

### MVP では対象外

- `str` / `array[str]`
- cHSP ブロック内の `ddim` / `sdim`
- cHSP ブロックから通常の HSP 関数を呼ぶこと
- cHSP ブロック内部での HSP プリプロセッサのマクロ展開
- `gettime` の cHSP ブロック内利用
- HSP の一般的な「任意位置の引数省略」

### 生成されるファイル

- **最適化AXスクリプト (`.ax`)**:
  - `#chsp_*`ブロックが、ネイティブコードで実装された機能を呼び出すHSPコードに置き換えられます。
  - ネイティブ側で処理される変数の受け渡し処理などが自動的に挿入されます。
- **Cソースコード (`.c`)**:
  - module ごとに target に応じた C ソースを生成します。
  - target 未指定の module は既定で plugin backend 用の C ソースを生成します。

### 変数共有

- MVP では引数として HSP の数値と数値配列をネイティブ側に渡します。
- HSP側のグローバル変数はネイティブ側では利用できません。
- cHSP ブロックから通常の HSP 関数は呼べません。

## 設計

### コンパイラ (hspcmp)

`hspcmp`は、`.chsp`ファイルを解釈し、`.ax`とネイティブソース (`.c`) を生成します。
既存文法だけの`.hsp`ファイルを受け取った場合は、既存のhspcmpと同様の処理を行います。

#### 処理フロー

1. **`.chsp`ファイルのパース**:
   - `#chsp_*`ブロックとそれ以外のHSPコードを分離します。
   - MVP ではこの段階を既存 HSP プリプロセッサより前に実行します。
2. **ネイティブコード生成**:
   - `#chsp_*`ブロック内のコードを C の関数に変換します。
   - 各 `#chsp_module` の target 指定に応じて、plugin backend の補助関数呼び出しか、素の C backend 向けコードに変換します。
3. **HSPコード生成**:
   - `#chsp_*`ブロックを、生成したネイティブ関数を呼び出す`#uselib`、`#func`、`#cfunc`命令に置き換えます。
4. **ファイル出力**:
   - `.ax`とネイティブソース (`.c`) を出力します。
5. **ネイティブコードのコンパイル**:
   - 生成された`.c`を、DLLや共有ライブラリにコンパイルします。
   - コンパイルされたライブラリは、生成された`.hsp` / `.ax`から呼び出されます。

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

`target=c` は C backend の試作で、`rnd` / `randomize` は `rand` / `srand` ベースです。`mt19937` (`HSPRANDMT`) には対応しません。

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

#chsp_defcfunc int add_one int v
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

#chsp_defcfunc int check_dl
    return can_open_self()
#chsp_end

#chsp_module_end
```

- `#chsp_clink` は module ごとに有効です。
- 1 行に 1 つのライブラリ名を書きます。複数必要な場合は複数行書きます。
- Linux では `#chsp_clink "dl"` のように、通常の `-l<name>` に相当する `<name>` 部分だけを書きます。
- `#chsp_clink` は `target=c` / `target=plugin` のどちらでも使えます。

#### mixed target を含む `.chsp` の出力方針

- `#chsp_module` ごとに独立した `.c` と共有ライブラリを生成します。
- 生成される HSP 側コードでは、module ごとに対応する `#uselib` を 1 回だけ出力します。
- 同一 `.chsp` 内で `target=plugin` と `target=c` を混在させられます。
- 出力ファイル名は module 単位で一意になる必要があります。
  - MVP では module の出現順を使い、`foo.chsp` から `foo_1.c` / `foo_1.so`、`foo_2.c` / `foo_2.so` のように出力します。

```sh
./hspcmp -d -i -u --compath=common/ sample/chsp/ao_opt.chsp
```

Linux では `sample/chsp/ao_opt.c` と `sample/chsp/ao_opt.so`、Win32 では `sample\chsp\ao_opt.c` と `sample\chsp\ao_opt.dll` がまとめて生成されます。

Win32 では `libtcc` のヘッダと import library を参照できるように、`src/hspcmp/win32/hspcmp.vcxproj` が `$(LIBTCC_DIR)\include` と `$(LIBTCC_DIR)\lib` を見に行きます。実行時の `libtcc` ランタイム探索は以下の順です。

- `LIBTCC_DIR`
- `hspcmp.exe` と同じディレクトリの `tcc\`

#### C backend の回帰テスト

`test/test_chsp_compare/Makefile` には C backend 用ターゲットがあります。

```sh
make -C test/test_chsp_compare check-emit-c
make -C test/test_chsp_compare check-emit-c-tcc
make -C test/test_chsp_compare check-libtcc
```

- `check-emit-c`
  - C backend を `cc` で共有ライブラリ化して、比較テストと transform テストを通します。
- `check-emit-c-tcc`
  - C backend を `tcc` で共有ライブラリ化して、同じ比較テストと transform テストを通します。
- `check-libtcc`
  - `hspcmp --chsp-compile=libtcc` で直接共有ライブラリを出力し、比較テストを通します。

mixed target 対応後は、少なくとも「plugin module のみ」「C module のみ」「plugin / C 混在」の 3 パターンを比較テストで通します。

### 生成された `.c` のビルド方法

生成された `.c` は、ビルド時には OpenHSP リポジトリのルートを include path に含めます。
Windows 向けの生成コードは `CHSP_EXPORT` マクロで `__declspec(dllexport)` が付くため、追加の `.def` は不要です。

以下では、リポジトリのルートで `sample/chsp/ao_opt.chsp` から `sample/chsp/ao_opt.c` を生成済みとします。

#### Linux

`.so` を生成します。

```sh
cc -std=c11 -O2 -shared -fPIC -I. -o sample/chsp/ao_opt.so sample/chsp/ao_opt.c -lm
```

`#chsp_clink` を使った module は、必要なライブラリを追加してビルドします。たとえば `#chsp_clink "dl"` を使っている場合は次のようになります。

```sh
cc -std=c11 -O2 -shared -fPIC -I. -o sample/chsp/dl_sample.so sample/chsp/dl_sample.c -lm -ldl
```

HSP 側の `#uselib` は Linux では `.so` を参照します。

```hsp
#uselib "ao_opt.so"
```

#### Win32

Visual Studio の `x86 Native Tools Command Prompt for VS` など、32bit 向けの MSVC 環境を開いてから `cl` を実行します。

```bat
cl /O2 /LD /I. /Fe:sample\chsp\ao_opt.dll sample\chsp\ao_opt.c
```

追加ライブラリが必要な場合は、`#chsp_clink` に対応する import library を `cl` の引数へ追加します。

HSP 側の `#uselib` は `.dll` を参照します。

```hsp
#uselib "ao_opt.dll"
```

#### Win64

Visual Studio の `x64 Native Tools Command Prompt for VS` など、64bit 向けの MSVC 環境を開いてから `cl` を実行します。

```bat
cl /O2 /LD /I. /Fe:sample\chsp\ao_opt.dll sample\chsp\ao_opt.c
```

出力ファイル名は Win32 と同じ `.dll` で問題ありません。32bit 用 HSP からは Win32 版 DLL、64bit 用 HSP からは Win64 版 DLL を読み込ませます。

#### 追加メモ

- `rnd` / `randomize` の挙動を HSP ランタイムと合わせたい場合は、必要に応じて `HSPRANDMT` を定義してビルドします。
- デバッグビルドにしたい場合は、Linux では `-g`、MSVC では `/Zi` を追加します。
- 生成された `.hsp` / `.ax` の `#uselib` に書かれたファイル名と、実際に生成した共有ライブラリのファイル名を一致させてください。

### HSPランタイムAPI

MVP では HSP SDK 連携は行わず、純粋な C ABI で受け渡し可能な型だけを対象にします。
将来的に HSP ランタイム連携を導入する場合は、`ddim` / `sdim`、文字列、HSP 関数呼び出しなどをこの層で扱います。

採用した backend 案の詳細は別ファイルに分離しています。

- plugin backend
  - [chsp-plugin-backend.md](chsp-plugin-backend.md)

### 組み込み関数

MVP では、組み込み関数名は HSP 名をそのまま受け付け、ネイティブ側では plugin backend の補助関数または C backend 向けの実装へ変換します。

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
このサンプルでは、`ao_original.hsp`（オリジナルのHSPスクリプト）の処理のうち、特に計算負荷の高いレイトレーシングの部分を`#chsp`ブロックに記述し、ネイティブコードに置き換えることで高速化を図っています (`ao_opt.chsp`)。

このように、cHSPは計算量の多い処理をネイティブコードにオフロードすることで、HSPスクリプトの実行速度を6倍程度に向上させることができる例を示しています。

### `ao_original.hsp` と `ao_opt.chsp` の主な変更点

`ao_opt.chsp`では、パフォーマンス向上のため、以下の関数が`#chsp`ブロックで囲われ、ネイティブコードとしてコンパイルされるように変更されています。

- ベクトル計算: `vdot`, `vcross`, `vnormalize` などのベクトル演算関数。
- レイとオブジェクトの交差判定: `ray_sphere_intersect`, `ray_plane_intersect` といった、レイトレーシングの中核となる関数。

これらの関数は、ピクセルごとに何度も呼び出されるため、ネイティブ化による高速化の効果が特に大きくなります。

DLLに分離したネイティブ関数として実装するため変更が必要です。

- 多次元配列を1次元配列に書き換える
- グローバル変数参照を関数の引数に書き換える

ソースコード:  `ao_opt.chsp`

```hsp
#chsp_deffunc vcross array[double] c, array[double] v0, array[double] v1
    c(0) = v0(1) * v1(2) - v0(2) * v1(1)
    c(1) = v0(2) * v1(0) - v0(0) * v1(2)
    c(2) = v0(0) * v1(1) - v0(1) * v1(0)
    return
#chsp_end
```

生成されるCコード: `ao_opt.c`

```c++
extern "C" CHSP_EXPORT void vcross(double *c, double *v0, double *v1) {
    c[0] = v0[1] * v1[2] - v0[2] * v1[1];
    c[1] = v0[2] * v1[0] - v0[0] * v1[2];
    c[2] = v0[0] * v1[1] - v0[1] * v1[0];
    return;
}
```

生成されるAXコードと等価なHSPコード: `ao_opt.hsp`

```hsp
#uselib "ao_opt.so"
// もしくは #uselib "ao_opt.dll"
#func global vcross "vcross" var, var, var
```

## 限界と課題

- **対応文法の選択**:
  - hsp側とc++側双方にラッパー処理を生成すれば制限を緩和できるか？
  - doubleやstrも返せるように出来るか検討
  - グローバル変数の参照を検出して自動で引数に変換する
    - 変数の型が分からない
- **実装上の課題**:
  - 専用コマンドとして実装するか、hspcmpのラッパーや拡張として実装するか
  - C++のコンパイルをどのように行うか
    - CMakeやMakefileを生成する
    - 直接C++コンパイラを呼び出す
- **生成C++のランタイムを決める**:
  - ライブラリ無しのC/C++製DLL生成:
    - `int` や `double` や `char` もしくはそのポインタをを受け取って、`int` または `void` を返す関数のみを対象にする。
    - HSP側の変数にはアクセスできない
    - C++からHSP側の関数は呼び出せない
    - オーバーヘッドがない
    - DLLは通常のC++プログラムからも利用可能
  - HSP SDKを使った拡張プラグインDLL生成:
    - 任意のパラメーターを受け取って任意の返り値を返せる
    - HSP側のユーザー定義関数の呼び出し可能(要調査)
    - HSP側の変数にアクセス可能(要調査)

現時点の実装は前者の「純粋な C ABI の DLL / 共有ライブラリ生成」です。

## 他の手法との比較

### hsp3cnv

- 特徴
  - AndroidやiOSターゲットのhsp3dish開発で使われるHSPからC++へのコンバーター
  - HSPと同様に型は緩い
  - HSPの文法をほぼそのまま使える

- 利点
  - HSPの文法をそのまま使える
  - 既に動作するものがある

- 欠点
  - 速度上のメリットはほぼない

### hsp3ll

- 特徴
  - HSPの文法をそのまま使えるLLVMベースのJITコンパイラ
  - LLVM IRに変換して実行する
  - 実行時の型情報を利用して型を推論し最適化する

- 利点
  - HSPの文法をそのまま使える
  - 実行時の型情報を利用した最適化で高速化可能

- 欠点
  - JITの実装コストが高い
  - 未完成
  - 最適化のためには各種hsp命令を型ごとに特殊化したLLVM IRライブラリが必要

### chsp

- 特徴
  - HSPの文法を拡張してC++コードに変換する
  - C++のコンパイラを使ってコンパイルする
  - 変数の型を明示的に指定する必要がある

- 利点
  - HSPの文法を拡張することで、C++コードに変換しやすい構文を提供
  - C++の型システムを利用して、パフォーマンスを向上させる
  - 既存のHSPスクリプトを部分的に高速化できる

- 欠点
  - HSPの文法を拡張するため、書き換えが必要
  - C++と同等の速度を得るためには、サンプルでの`fabs`や`sqrt`のようにHSPの命令とは別にC++で実装かマッピングが必要
  - コンパイルエラーや実行時エラーのデバッグにはC++の知識が必要
  - コンパイルにC++のコンパイラが必要
