# cHSP ユーザーズガイド

## 概要

cHSPは、HSPスクリプトの一部をネイティブCコードに変換し、コンパイル・実行することでスクリプトの実行速度を向上させるための拡張フロントエンドです。

cHSPを有効にしたHSPエディタ、またはコマンドラインの`chsp.exe`から使用できます。この文書ではHSP 3.7 Win32追加パッケージの利用方法を説明します。以下の配置パスは、特に断りがなければ展開先の`hsp37/`を基準とします。

### 主な特徴

- **ネイティブCへの変換と内蔵コンパイラ**:
  - 高速化したい関数ブロックをネイティブCコードに変換します。
  - バックエンドとして **`libtcc` (Tiny C Compiler)** を内蔵しており、外部C/C++コンパイラを別途インストールすることなく共有ライブラリ（Linux: `.so`, Windows: `.dll`）を自動ビルドします。
- **HSP3プラグインアーキテクチャによるシームレスな統合**:
  - 生成されたネイティブコードは既定でHSP3プラグインバックエンド（`hsp3cmdinit` / `#regcmd`）形式を採用しており、オーバーヘッドを最小限に抑えつつ配列や数値データを安全にやり取りできます。
- **既存スクリプトとの互換性と透過的委譲**:
  - 通常の `.hsp` はそのまま `hspcmp` へ素通しします。
  - `#chsp_*` ブロックを含むスクリプトは、ネイティブライブラリ生成および中間コード生成後、`hspcmp` を実行して `.ax` を出力します。
  - 空行・マーカー埋め込みにより、エラー発生時も元の `.chsp` の行番号が正確に保持されます。

---

## 導入と使い方

### Windows配布パッケージの導入

配布ZIPは公式HSP 3.7の32bit版に追加して使います。cHSP自体のビルドや外部Cコンパイラのインストールは不要です。

1. 公式の`hsp37.zip`を展開します。
2. cHSPのZIPを同じ場所に展開し、最上位の`hsp37/`を重ねます。
3. HSPエディタを終了して、`hsp37/enable_chsp.bat`を実行します。
4. エディタで`sample/chsp/hello.hsp`を開き、F5で`42`と表示されることを確認します。

ZIPの展開だけでは公式コンパイラは変更されません。有効化バッチは公式`hspcmp.dll`を`hspcmp_original.dll`にリネームし、`hspcmp_chsp.dll`を`hspcmp.dll`へコピーします。退避DLLはchspからの委譲にも必要なので削除しないでください。

公式版に戻す場合はエディタを終了して`disable_chsp.bat`を実行します。更新は「旧パッケージで無効化 → 新ZIPを展開 → 有効化」の順です。CLIだけを使う場合はDLLの切替は不要です。

この文書は配布ZIPの`doclib/chsp.txt`にも収録しています。ディレクティブ別のヘルプは`hsphelp/chsp.hs`にあり、ヘルプビューアーを再起動して検索できます。詳しい導入手順はパッケージ直下の`README_CHSP.txt`を参照してください。

### CLIからの実行

通常の `hspcmp` と同様のオプションで実行可能です。

```text
chsp.exe [options] <source.chsp|source.hsp>
```

**主なオプション:**

| オプション | 説明 |
| :--- | :--- |
| `-o<file>` | 出力ファイル名を指定（通常は `.ax`、`--library` では `.as`） |
| `--library` | include 用の `.as` とネイティブライブラリを生成。hspcmp への委譲・`.ax` 生成は行わない |
| `-d` | デバッグ情報を付加 |
| `-i` | 入力ソースをUTF-8として読み込む |
| `-u` | 文字列をUTF-8で出力する（対応するランタイムで実行） |
| `--compath=<path>` | 共通ディレクトリを指定。CLI の既定値は作業ディレクトリ基準の `common/` |
| `--chsp-compile=libtcc\|none` | `libtcc` は共有ライブラリまで生成（既定）。`none` は C ソースを生成し、共有ライブラリのコンパイルを省略 |
| `--keep-tmp` | CLI が通常削除する `<入力名>.chsp.tmp.hsp` を保持。生成された `.c` と共有ライブラリは指定なしでも残る |
| `--hspcmp=<path>` | 委譲先の `hspcmp` バイナリのパスを指定 |

#### Windows CLI の必要ファイルと実行例

以下は `C:\hsp37` に cHSP を配置し、ソースのある別ディレクトリから実行する例です。実際の配置先に合わせてパスを変更してください。

- `chsp.exe` と、それが使用する `libtcc.dll`。
- 委譲先の通常版 `hspcmp.exe` と、実行用の HSP ランタイム（この例では `hsp3cl.exe`）。生成コード・使用機能に対応するバージョンとアーキテクチャを揃えます。
- `common/` 一式。標準 HSP ヘッダーに加え、`common/chsp/chsp_builtins.tsv`、`chsp_runtime.h`、プラグイン SDK ヘッダーなどの `common/chsp/` 一式が必要です。
- libtcc のヘッダー・ライブラリを含むランタイム一式。`chsp.exe` と同じディレクトリの `tcc/` に配置するか、環境変数 `LIBTCC_DIR` でそのディレクトリを指定します。`libtcc.dll` だけでは生成 C コードのコンパイルに必要なファイルが揃いません。

作業ディレクトリに `answer.hsp` を作成します。現状は高速化対象の定義も入口ファイルに直接記述します。

```hsp
#chsp_module "answer_native"
#chsp_defcfunc int answer int p_value
    return p_value + 1
#chsp_end
#chsp_module_end

mes answer(41)
end
```

PowerShell で実行します。

```powershell
& "C:/hsp37/chsp.exe" --compath=C:/hsp37/common/ --hspcmp=C:/hsp37/hspcmp.exe -i -u -d --keep-tmp answer.hsp
if ($LASTEXITCODE -eq 0) {
    & "C:/hsp37/hsp3cl.exe" answer.ax
}
```

実行結果は `42` です。`answer.ax`、`answer_native.c`、`answer_native.dll` と、`--keep-tmp` により保持される `answer.chsp.tmp.hsp` を確認できます。Windows の libtcc は `.def` を生成する場合もあります。既定の `target=plugin` では `#regcmd` / `#cmd` を含む呼び出しコードを生成し、`target=c` では `#uselib` / `#func` / `#cfunc` を使用します。

`--compath` を省略すると、インストール先ではなく作業ディレクトリの `common/` を参照します。別ディレクトリから実行する場合は、上のように絶対パスと末尾の `/` を指定してください。`--chsp-compile=none` は生成 C コードの確認や外部コンパイラでのビルドに使います。通常の hspcmp への委譲は行われますが、実行に必要な共有ライブラリは別途用意する必要があります。

現在のWindows CLIには、空白を含む配置パスで委譲先の`hspcmp.exe`を起動できない制限があります。CLIを使う場合は空白を含まない場所へ展開してください。配布版はWin32用で、生成DLLも32bitランタイムで使用します。

`LIBTCC_DIR`を設定すると同梱の`tcc/`より指定先が優先されます。Proxy DLLでは`HSPCMP_ORIGINAL`で指定した委譲先DLLも優先されます。同梱構成を使う場合は、これらの環境変数を未設定にしてください。

#### include 用ライブラリの生成

`--library` を指定すると、入口ファイルの cHSP 定義をネイティブ化し、通常の HSP から include できる `.as` を出力します。`--keep-tmp` と委譲先の `hspcmp.exe` は不要です。

例えば上の `answer.hsp` から末尾の `mes answer(41)` と `end` を除いた定義を `answer.chsp` に保存し、次を実行します。

```powershell
& "C:/hsp37/chsp.exe" --library --compath=C:/hsp37/common/ answer.chsp
```

`answer.as` と `answer_native.c`、`answer_native.dll` が生成されます。通常の hspcmp でビルドする利用側は次のように記述できます。

```hsp
#include "answer.as"
mes answer(41)
end
```

- `.as` は入力名を基に生成します。`-oout/answer.as` で出力先を指定すると、`.c` と共有ライブラリもそのディレクトリに生成します。出力先ディレクトリは事前に作成してください。
- 共有ライブラリ名は各 `#chsp_module` の名前に従います。1入力に複数の module があれば複数の共有ライブラリを生成します。別ライブラリと module 名・公開関数名が重複しないようにしてください。
- module 外の HSP コードは順序を保って `.as` に残します。初期化呼び出しなどは生成時には実行されず、利用側で include した位置のコードとして実行されます。`end` などもそのまま残る点に注意してください。
- include ガードにより、同じ生成 `.as` の重複登録・コードの重複挿入を防ぎます。実行時に include 位置へ繰り返し制御が戻る場合の「初期化を一度だけ実行する」保証ではありません。
- `.as` の探索と実行時の DLL 探索は別です。最初は `.as`・DLL・利用側の実行ファイルを同じディレクトリに配置してください。元コードの相対 `#include` やリソース参照は書き換えません。出力先を変更する場合は関連ファイルの配置も調整してください。
- `--chsp-compile=none` と併用すると `.as` と `.c` だけを生成します。実行前に共有ライブラリを別途ビルドしてください。

これは include **先のソースを自動変換する機能ではありません**。元の `.chsp` を変更したらライブラリを再生成し、利用側も再コンパイルしてください。生成される `.as` は UTF-8 なので、利用側も `-i` を指定してビルドします。

---

## 基本仕様と構文

`.hsp`と`.chsp`のどちらでもcHSPの拡張構文を使用できます。エディタで開く配布サンプルは`.hsp`です。拡張構文を含まないファイルは通常のHSPとしてコンパイルします。

### モジュールと関数定義

高速化対象の処理を `#chsp_module` 〜 `#chsp_module_end` で囲み、その中に関数を定義します。

```hsp
#chsp_module "my_math"

// 戻り値のある関数 (#chsp_defcfunc <戻り値型> <関数名> <引数...>)
#chsp_defcfunc double vdot array[double] v0, array[double] v1
    return v0(0) * v1(0) + v0(1) * v1(1) + v0(2) * v1(2)
#chsp_end

// 戻り値のない命令 (#chsp_deffunc <命令名> <引数...>)
#chsp_deffunc vcross array[double] c, array[double] v0, array[double] v1
    c(0) = v0(1) * v1(2) - v0(2) * v1(1)
    c(1) = v0(2) * v1(0) - v0(0) * v1(2)
    c(2) = v0(0) * v1(1) - v0(1) * v1(0)
    return
#chsp_end

#chsp_module_end
```

`#chsp_defcfunc`の戻り値型は`int`または`double`です。`#chsp_deffunc`は戻り値のない命令を定義します。各関数の定義は`#chsp_end`、モジュール全体は`#chsp_module_end`で終了します。

### モジュールの出力名とtarget指定

`#chsp_module "my_math" target=plugin`のように、モジュールごとに出力名と方式を指定できます。

- `target=plugin`（省略時の既定値）: HSP3プラグイン形式で生成します。通常はこちらを使用します。
- `target=c`: 通常のC関数を公開する形式で生成します。
- モジュール名の文字列は省略可能です。例えば`math.hsp`内の名前なしモジュールは、出現順に`math_1`、`math_2`などの出力名になります。
- 1ファイルに複数のモジュールを定義でき、異なるtargetを混在させられます。モジュールごとに`.c`と共有ライブラリ（Windowsでは`.dll`、Linuxでは`.so`）を生成します。出力名は重複させないでください。

targetは`#chsp_module`行で指定します。CLIの`--chsp-compile=libtcc|none`は、生成Cコードから共有ライブラリをビルドするかどうかの指定です。既定では`libtcc`で共有ライブラリまで生成します。

### サポートする型

cHSP ブロック内の引数およびローカル変数は型指定が必須です。

- **基本型**: `int`, `double`
- **引数配列**: `array[int]`, `array[double]`
  - `a(i)`から`a(i, j, k, l)`まで、最大4次元の配列アクセスに対応しています。
- **ローカル変数・固定長配列**:
  - `local[int]`, `local[double]`
  - `local[int[n]]`, `local[double[n]]` (1次元の固定長配列)

### 対応する文・式・組み込み関数

- 代入、`+=`、`-=`、`*=`、`/=`などの複合代入
- `if` / `else if` / `else`、`if 条件 : 文`の1行形式
- `repeat` / `loop` / `continue` / `break`、`return`
- 算術・比較・ビット・シフト演算、数値配列アクセス、関数呼び出し

主な組み込み関数は`int`、`double`、`length`、`length2`、`length3`、`length4`、`abs`、`absf`、`sin`、`cos`、`tan`、`atan`、`sqrt`、`expf`、`logf`、`powf`、`limit`、`limitf`、`rnd`です。`randomize`も使用できます。

`rnd` / `randomize`の乱数状態は通常のHSP側とは独立しています。HSP側の`randomize`ではcHSP側の乱数状態を初期化できません。`target=c`はCの`rand` / `srand`を使用し、Mersenne Twisterには対応しません。両側で同じ乱数列になることを前提にしないでください。

### インライン C コードの埋め込み

`#chsp_c` を使うことで、ネイティブ C コードを直接記述して標準ライブラリ（`math.h` 等）や最適化ルーチンを利用できます。

```hsp
#chsp_module "native_sample"

#chsp_c {"
#include <math.h>
double c_distance(double x, double y) {
    return sqrt(x * x + y * y);
}
"}
#chsp_cdecl c_distance

#chsp_defcfunc double calc_dist double x, double y
    return c_distance(x, y)
#chsp_end

#chsp_module_end
```

`#chsp_c`はモジュール内で使用します。C関数をcHSPから呼ぶには`#chsp_cdecl`で名前を登録し、Cの定義と同じ大文字・小文字で記述してください。`#chsp_cdecl`はモジュール内かつcHSP関数本体の外に置きます。どちらも`target=plugin`と`target=c`で利用できます。

### 追加ライブラリのリンク

生成DLLが追加のライブラリを必要とする場合は、モジュール内に`#chsp_clink "name"`を記述します。指定はそのモジュールだけに適用され、1行に1ライブラリ、複数必要なら複数行で指定します。両方のtargetで使用できます。

WindowsではTCCから参照できるライブラリ名（例: `"msvcrt"`）、Linuxでは`-l`に続ける名前（例: `"dl"`）を指定します。対象OS向けのライブラリが必要です。`--chsp-compile=none`で出力したCコードを外部コンパイラでビルドする場合は、対応するリンク指定も自分で追加してください。

### 現在の主な制約

- `#include` 先の `#chsp_*` 定義は変換されません。入口ファイルに直接記述してください。入口に空の `#chsp_module` を追加しても解決しません。
- この制限はCLIとProxy DLLの両方にあり、現状コンパイルエラーとして検出されず、コンパイルが成功しても未初期化変数の警告や実行時 Error 10 になる場合があります。通常の HSP の include は委譲先の hspcmp が処理します。分割する場合は、この文書の「include 用ライブラリの生成」の手順で`--library`から`.as`とDLLを先に生成してください。終了コード0だけではinclude先がネイティブ化されたことを確認できません。

- HSP 側のグローバル変数はネイティブ側から直接アクセスできません（関数の引数経由で渡す必要があります）。
- cHSP ブロック内から通常の HSP ユーザー定義関数や標準 GUI 命令（`mes`, `pos` 等）は呼び出せません。
- `str` / `array[str]`、cHSPブロック内の`ddim` / `sdim`、`gettime`には対応していません。
- cHSPブロック内部でのHSPプリプロセッサによるマクロ展開と、一般的な任意位置の引数省略には対応していません。
- `int`と`double`の混在演算、および戻り値・代入時の暗黙変換の意味は仕様策定中です。現状はCの型変換規則に依存する場合があるため、通常のHSPと同じ結果になることを前提にしないでください。

---

## サンプル: aobench

`sample/chsp/ao_opt.hsp`は、アンビエントオクルージョンの3DCGレンダリングベンチマークプログラムです。

比較用の通常HSP版`sample/chsp/ao_original.hsp`のうち、ピクセルごとに繰り返し呼び出されるベクトル計算およびレイと球・平面の交差判定を`#chsp_module`化しています。

### Windows配布版での実行

cHSPを有効にしたエディタで`sample/chsp/ao_opt.hsp`を開き、F5で実行してください。画面描画を使うため、GUI版のHSPランタイムで実行します。

計算負荷の高い部分をネイティブ実行する効果を比較できます。実行速度は環境や設定によって異なります。

---

## その他の配布サンプルとヘルプ

`sample/chsp/hello.hsp`は最小例です。`sample/chsp_test/`には数値・配列・分岐の比較例があります。`*_hsp.hsp`は通常HSP、`*_chsp_c.hsp`は`target=c`、`*_chsp_p.hsp`は`target=plugin`です。同じ名前の`.gt`には期待するテキスト出力を収録しています。

ヘルプビューアーでは、`#chsp_module`、`#chsp_module_end`、`#chsp_deffunc`、`#chsp_defcfunc`、`#chsp_end`、`#chsp_c`、`#chsp_cdecl`、`#chsp_clink`を検索できます。
