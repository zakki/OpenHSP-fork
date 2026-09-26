# cHSP ユーザーズガイド

## 概要

cHSPは、HSPスクリプトの一部をネイティブCコードに変換し、内蔵コンパイラで自動コンパイル・実行することで、スクリプトの実行速度を向上させるための拡張フロントエンドです。

この文書ではHSP 3.7 Win32追加パッケージの利用方法を説明します。以下の配置パスは、特に断りがなければ展開先の `hsp37/` を基準とします。cHSPを有効にしたHSPエディタ、またはコマンドラインの `chsp.exe` から使用できます。

### 主な特徴

- ネイティブCへの変換と内蔵コンパイラ:
  - 高速化したい関数ブロックをネイティブCコードに変換します。
  - バックエンドとして `libtcc` (Tiny C Compiler) を内蔵しており、外部Cコンパイラを別途インストールすることなく共有ライブラリ（DLL）を自動ビルドします。
- HSP3プラグイン形式による連携:
  - 生成されたネイティブコードは既定でHSP3プラグイン形式（`target=plugin`）を採用しており、オーバーヘッドを抑えて配列や数値をやり取りできます。
- 既存スクリプトとの互換性:
  - 通常のHSPコードはそのまま公式の `hspcmp` へ素通しします。
  - 空行・マーカー埋め込みにより、エラー発生時も元のスクリプトの行番号が保持されます。

---

## 導入とクイックスタート（エディタでの利用）

### 導入手順

配布ZIPは公式HSP 3.7の32bit版に追加して使います。cHSP自体のビルドや外部Cコンパイラのインストールは不要です。

1. 公式の `hsp37.zip` を展開します。
2. cHSPのZIPを展開し、最上位の `hsp37/` フォルダを重ねます。
3. HSPエディタを終了して、`hsp37/enable_chsp.bat` を実行します。
4. HSPエディタで `sample/chsp/hello.hsp` を開き、F5キーを押して `42` と表示されることを確認します。

> 有効化・復元の仕組み:
> ZIPの展開だけでは公式コンパイラは変更されません。`enable_chsp.bat` は公式 `hspcmp.dll` を `hspcmp_original.dll` にバックアップし、chsp版コンパイラを配置します。退避DLLは委譲実行にも必要ですので削除しないでください。
> 公式版に戻す場合はエディタを終了して `disable_chsp.bat` を実行します。

---

## cHSPスクリプトの書き方

通常の `.hsp` スクリプト内に、高速化したい処理を `#chsp_module` 〜 `#chsp_module_end` で囲んで記述します。

### モジュールと関数の基本定義

```hsp
#chsp_module "my_math"

// 戻り値のある関数 (#chsp_defcfunc 戻り値型 関数名 型 引数名, ...)
#chsp_defcfunc double vdot array[double] v0, array[double] v1
    return v0(0) * v1(0) + v0(1) * v1(1) + v0(2) * v1(2)
#chsp_end

// 戻り値のない命令 (#chsp_deffunc 命令名 型 引数名, ...)
#chsp_deffunc vcross array[double] c, array[double] v0, array[double] v1
    c(0) = v0(1) * v1(2) - v0(2) * v1(1)
    c(1) = v0(2) * v1(0) - v0(0) * v1(2)
    c(2) = v0(0) * v1(1) - v0(1) * v1(0)
    return
#chsp_end

#chsp_module_end

// 通常のHSPコードから呼び出し
ddim v1, 3 : v1(0) = 1.0, 2.0, 3.0
ddim v2, 3 : v2(0) = 4.0, 5.0, 6.0
ddim c, 3

vcross c, v1, v2
mes "cross product: " + c(0) + ", " + c(1) + ", " + c(2)
mes "dot product: " + vdot(v1, v2)
stop
```

- `#chsp_module "モジュール名"`:
  ネイティブ化するモジュールを開始します。モジュール名（文字列）は必須です。モジュール名を基にCソースとDLLを生成します。
  - 出力方式として `target=plugin`（既定値）と `target=c` を指定できます。通常のスクリプト記述では省略（plugin）で使用します。
- `#chsp_defcfunc 戻り値型 関数名 ...`:
  数値を返す関数を定義します。式の中で呼び出します。
- `#chsp_deffunc 命令名 ...`:
  戻り値のない命令を定義します。文として呼び出します。
- `#chsp_end`:
  各関数の定義を終了します。
- `#chsp_module_end`:
  モジュールブロックを終了します。

### サポートする型とローカル変数

cHSPブロック内の引数およびローカル変数は型指定が必要です。

| 分類 | 指定形式 | 説明 |
| :--- | :--- | :--- |
| 基本型 | `int`, `double` | 整数、実数値を受け取るスカラー引数 |
| 引数配列 | `array[int]`, `array[double]` | HSP側の配列を受け取る引数（最大4次元までアクセス可能） |
| ローカル変数 | `local[int]`, `local[double]` | 関数内でのみ使用する作業用スカラー変数 |
| 固定長ローカル配列 | `local[int[n]]`, `local[double[n]]` | 関数内でのみ使用する固定長配列（最大4次元まで宣言可能） |

#### ローカル変数の宣言と使用例

ローカル変数は、引数リストの末尾に `local[...]` として並べて宣言します。

```hsp
#chsp_module "calc_sample"

// 引数2つ、ローカル変数2つ（スカラー sum と 2次元ローカル配列 table）を持つ命令
#chsp_deffunc process_matrix array[int] out_arr, int count, local[int] sum, local[int[4][4]] table
    // ローカル配列への代入
    table(0, 0) = 10 : table(1, 1) = 20

    // 計算処理
    sum = 0
    repeat count
        sum += table(0, 0) * cnt
    loop

    out_arr(0) = sum
    return
#chsp_end

#chsp_module_end

dim result, 1
process_matrix result, 5
mes "result = " + result(0)
stop
```

- スカラーのローカル変数は `local[int] 変数名` や `local[double] 変数名` と記述します（初期値は `0` / `0.0`）。
- 固定長ローカル配列は `local[int[10]]`（1次元）や `local[int[4][4]]`（2次元）、`local[double[2][3][4]]`（3次元）のように最大4次元まで宣言できます。

### 対応する構文・演算子・組み込み関数

cHSP関数内では、以下のHSP構文・式を使用できます。

- 代入・演算:
  - 通常の代入（`x = 10`）
  - カンマ区切りの連続代入（`arr = 1, 2, 3` や `arr(0) = 10, 20`）
  - 複合代入（`+=`, `-=`, `*=`, `/=`）
  - インクリメント・デクリメント（`x++`, `x--`）
- 制御構文:
  - `if` / `else if` / `else`（ブロック形式および `if 条件 : 文` の1行形式）
  - `repeat ループ回数` 〜 `loop`
  - ループ制御: `break`, `continue`
  - ループカウンタ `cnt`: `repeat` 内で現在のループ回数（0始まり）を参照できます。
  - `return`（命令では単独、関数では `return 式`）
- 演算子:
  - 四則演算（`+`, `-`, `*`, `/`）
  - 剰余演算子 `\`（例: `a \ b`）
  - 比較演算子（`==`, `=`, `!=`, `!`, `<`, `>`, `<=`, `>=`）
  - 論理演算・ビット演算（`&`, `|`, `^`）
  - シフト演算（`<<`, `>>`）
- 配列要素数の取得:
  - `length(a)`, `length2(a)`, `length3(a)`, `length4(a)`
- 主な組み込み関数:
  - 数学関数: `abs`, `absf`, `sin`, `cos`, `tan`, `atan`, `sqrt`, `expf`, `logf`, `powf`, `limit`, `limitf`
  - 型変換: `int(x)`, `double(x)`
  - 乱数: `rnd(範囲)`, `randomize`（引数省略または `randomize シード値`）

> 乱数に関する注意:
> cHSPブロック内の `rnd` / `randomize` はCランタイムの標準乱数（`rand` / `srand`）を使用します。HSP側の標準乱数（Mersenne Twister）やHSP側の `randomize` とは独立して動作します。両側で同じ乱数列になることを前提にしないでください。

---

## HSPスクリプト高速化のポイント（実践ガイド）

### 高速化に適した処理

HSPのボトルネックになりやすい以下の処理を `#chsp_module` 化すると効果的です。

- ピクセル単位・頂点単位のループ計算（画像処理、3D演算、レイトレーシング）
- 多次元配列の走査やデータ変換
- 物理シミュレーションや数値計算

### 既存コードを移植する際のチェックリスト

1. グローバル変数を直接参照しない:
   cHSPブロック内からHSP側のグローバル変数は直接参照できません。必要な値・配列は引数で渡してください。
2. GUI命令や文字列処理を含めない:
   `mes`、`color`、`redraw` などのGUI描画命令や文字列操作は呼び出せません。描画や画面更新はHSP側で行い、計算処理をcHSP関数に切り出してください。
3. 作業用変数は `local[...]` で宣言する:
   関数内で使う変数は、引数リストの末尾に `local[int] i` などの形式で宣言してください。
4. 配列型を明示する:
   引数配列には `array[int]` または `array[double]` を指定してください。

### サンプル: aobench (アンビエントオクルージョン)

パッケージ内の `sample/chsp/ao_opt.hsp` は、3DCGのレンダリングベンチマークプログラムです。
比較用の通常HSP版 `sample/chsp/ao_original.hsp` のうち、ピクセルごとに繰り返し実行されるベクトル演算およびレイ交差判定を `#chsp_module` 化しています。

HSPエディタで `ao_opt.hsp` を開いて F5 を押すことで、ネイティブ実行の効果を比較できます。

---

## 注意事項と現在の主な制約

- `#include` 先の `#chsp_*` 定義は変換されません:
  高速化する定義は入口の `.hsp` ファイルへ直接記述してください。`#include` 先の chsp 定義は自動変換されず、未初期化エラーや実行時エラーの原因になります。分割する場合は、後述の「include 用ライブラリの生成」の手順で先にライブラリを生成してください。
- HSP 3.7 との組み合わせに関する制約:
  - `int64` 型や `lldim` などの 64bit 整数機能は HSP 3.8 開発版向けであり、HSP 3.7 環境では利用できません（`int` または `double` を使用してください）。
  - 配布パッケージは 32bit (Win32) 向けです。64bit ランタイム用 DLL は生成しません。
- 未対応の型・機能:
  - 文字列型（`str`）および文字列配列、ブロック内での `sdim` / `ddim`、`gettime` には対応していません。
  - cHSPブロック内でのHSPマクロ展開や、任意位置の引数省略には対応していません。
- 型変換・演算の規則:
  - `int` と `double` の混在演算や暗黙の型変換は、バックエンドの C 言語の型変換規則に準拠します。厳密な型精度が求められる計算では、明示的に `int()` や `double()` でキャストしてください。

---

## 高度な使い方

### インライン C コードの埋め込み

`#chsp_c` を使うことで、モジュール内にネイティブ C コードを直接記述し、Cの標準関数や外部最適化ルーチンを利用できます。

```hsp
#chsp_module "native_sample"

// Cコードをそのまま埋め込む
#chsp_c {"
#include <math.h>
double c_distance(double x, double y) {
    return sqrt(x * x + y * y);
}
"}

// 埋め込んだC関数をcHSPから呼べるように登録
#chsp_cdecl c_distance

#chsp_defcfunc double calc_dist double x, double y
    return c_distance(x, y)
#chsp_end

#chsp_module_end

mes "dist = " + calc_dist(3.0, 4.0)
stop
```

- `#chsp_c` はモジュール内に記述します。
- C関数を呼び出すには、モジュール内・関数定義の外に `#chsp_cdecl C関数名` を記述して登録します（大文字・小文字を厳密に一致させてください）。

### 追加ライブラリのリンク

生成DLLが追加のCライブラリを必要とする場合は、モジュール内に `#chsp_clink "ライブラリ名"` を記述します。

```hsp
#chsp_module "native_math" target=c
#chsp_clink "msvcrt"
#chsp_defcfunc double calc_root double p_value
    return sqrt(p_value)
#chsp_end
#chsp_module_end

mes "calc_root(16.0) = " + calc_root(16.0)
stop
```

### コマンドライン（CLI）からの実行

コマンドプロンプトや PowerShell から `chsp.exe` を直接実行することも可能です。CLI利用時はDLLの切り替え（`enable_chsp.bat`）は不要です。

```text
chsp.exe [options] <source.chsp|source.hsp>
```

主なオプション:

| オプション | 説明 |
| :--- | :--- |
| `-o<file>` | 出力ファイル名を指定（通常は `.ax`、`--library` では `.as`） |
| `--library` | include用の `.as` とネイティブライブラリを生成（`.ax` は生成しない） |
| `-d` | デバッグ情報を付加 |
| `-i` | 入力ソースを UTF-8 として読み込む |
| `-u` | 文字列を UTF-8 で出力する |
| `--compath=<path>` | 共通ディレクトリを指定（既定値は作業ディレクトリ基準の `common/`） |
| `--chsp-compile=libtcc\|none` | `libtcc` は共有ライブラリまで自動ビルド（既定）。`none` はCソースの出力のみ |
| `--keep-tmp` | 中間ファイル `<入力名>.chsp.tmp.hsp` を削除せず保持する |
| `--hspcmp=<path>` | 委譲先の `hspcmp` バイナリのパスを指定 |

> CLI利用時の注意:
> - 別の作業ディレクトリから実行する場合は、`chsp.exe`、`--compath`、`--hspcmp` に絶対パスを指定してください（末尾の `/` も必要です）。
> - 現在の Windows CLI には、空白を含むパスで委譲先の `hspcmp.exe` を起動できない制限があります。CLI を使う場合は、空白を含まないディレクトリへ展開してください。

### include 用ライブラリの生成（`--library`）

複数のスクリプトからネイティブ関数を共通利用したい場合や、ファイルを分割したい場合は、`--library` オプションを使って事前に `.as` とネイティブ DLL を生成します。

1. 高速化対象の定義だけを記述したファイル（例: `answer.chsp`）を作成します。
2. 次のコマンドを実行します：
   ```powershell
   chsp.exe --library --compath=C:/hsp37/common/ answer.chsp
   ```
3. `answer.as` と `answer_native.dll` が生成されます。
4. 通常の HSP スクリプトから `#include "answer.as"` することで、通常の `hspcmp` でコンパイル・実行が可能になります。

---

## 配布サンプルとヘルプ一覧

- サンプルスクリプト:
  - `sample/chsp/hello.hsp`: 導入確認用の最小例
  - `sample/chsp/ao_opt.hsp`: 高速化した aobench
  - `sample/chsp/ao_original.hsp`: 比較用の通常 HSP 版 aobench
  - `sample/chsp_test/`: 数値・配列・分岐等の網羅的な比較テスト例
- HSPヘルプビューアー:
  - `hsphelp/chsp.hs` が同梱されており、ヘルプビューアー（またはエディタで F1 キー）から `#chsp_module`、`#chsp_deffunc`、`#chsp_defcfunc` などの各ディレクティブを直接検索できます。
