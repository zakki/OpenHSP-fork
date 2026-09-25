# cHSP: HSPスクリプト高速化のための拡張フロントエンド

## 概要

cHSPは、HSPスクリプトの一部をネイティブCコードに変換し、コンパイル・実行することでスクリプトの実行速度を向上させるための拡張フロントエンドです。

独立したコンパイラフロントエンド（Linux: `chsp` CLI / Windows: `chsp.exe` CLI および `hspcmp.dll` Proxy DLL）として動作し、ネイティブトランスパイルから `hspcmp` への委譲までを透過的に実行します。

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

## ビルドと使い方

### 1. ビルド

リポジトリルートで make を実行します。

```bash
make chsp
```

ビルドが完了すると、リポジトリルートに CLI バイナリ `chsp` が生成されます。

### 2. 実行

通常の `hspcmp` と同様のオプションで実行可能です。

```bash
./chsp [options] <source.chsp|source.hsp>
```

**主なオプション:**

| オプション | 説明 |
| :--- | :--- |
| `-o<file>` | 出力ファイル名 (`.ax`) を指定 |
| `-d` | デバッグ情報を付加 |
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

### 3. テストの実行

```bash
make test-chsp
```

全テストマトリクス（通常モード、Cエミットモード、libtccネイティブ実行モード）が実行されます。

---

## 基本仕様と構文

詳細な仕様および対応命令一覧は [docs/chsp.md](docs/chsp.md) を参照してください。

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

### サポートする型

cHSP ブロック内の引数およびローカル変数は型指定が必須です。

- **基本型**: `int`, `double`
- **引数配列**: `array[int]`, `array[double]`
  - 多次元配列アクセス（`a(i, j)` や `a(i, j, k)` 等）に対応しています。
- **ローカル変数・固定長配列**:
  - `local[int]`, `local[double]`
  - `local[int[n]]`, `local[double[n]]` (固定長配列)

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

### 現在の主な制約

- `#include` 先の `#chsp_*` 定義は変換されません。入口ファイルに直接記述してください。入口に空の `#chsp_module` を追加しても解決しません。
- この制限は現状コンパイルエラーとして検出されず、コンパイルが成功しても未初期化変数の警告や実行時 Error 10 になる場合があります。通常の HSP の include は委譲先の hspcmp が処理します。詳細と分割ビルドの回避策は [include の制限](docs/chsp.md#include-の制限と分割ビルド) を参照してください。

- HSP 側のグローバル変数はネイティブ側から直接アクセスできません（関数の引数経由で渡す必要があります）。
- cHSP ブロック内から通常の HSP ユーザー定義関数や標準 GUI 命令（`mes`, `pos` 等）は呼び出せません。

---

## サンプル: aobench (`sample/ao_opt.chsp`)

[`sample/ao_opt.chsp`](sample/ao_opt.chsp) は、アンビエントオクルージョンの 3DCG レンダリングベンチマークプログラムです。

オリジナルの HSP スクリプト ([`sample/ao_original.hsp`](sample/ao_original.hsp)) のうち、ピクセルごとに膨大な回数呼び出されるベクトル計算およびレイと球・平面の交差判定を `#chsp_module` 化しています。

### コンパイルと実行例

```bash
# 1. chsp でコンパイル (.ax と .so が生成される)
./chsp sample/ao_opt.chsp

# 2. hsp3cl 等のランタイムで実行
hsp3cl ao_opt.ax
```

純粋な HSP スクリプトと比較して、計算負荷の高いレイトレーシング処理がネイティブ実行されることで **約6倍の高速化** を達成します。

---

## 他の手法との比較

| 方式 | 概要 | 利点 | 課題・欠点 |
| :--- | :--- | :--- | :--- |
| **hsp3cnv** | スクリプト全体を C++ に変換するコンバーター | HSP 文法をそのまま流用可能 | HSP ランタイムの動的型システムを模倣するため計算速度の向上幅が限定的 |
| **hsp3ll** | LLVM ベースの JIT コンパイラ | HSP 文法をそのまま高速化可能 | JIT 実装コストが高く未完成、型推論・命令特殊化の規模が大きい |
| **cHSP (本方式)** | ホットスポットを静的型付け C コードに変換し libtcc でコンパイル | **既存 HSP に局所導入可能**、C 言語と同等の最高速を発揮、**libtcc 内蔵により追加コンパイラ不要** | `#chsp_*` ブロック内の型宣言・書き換えが必要 |

---

## 関連ドキュメント

- [docs/chsp.md](docs/chsp.md): cHSP の言語仕様・文法・対応構文リファレンス
- [docs/chsp-internals.md](docs/chsp-internals.md): コンパイラ内部構造・AST エミッター・バックエンド設計
