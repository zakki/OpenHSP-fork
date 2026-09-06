# cHSP: HSPスクリプト高速化のための拡張フロントエンド

## 概要

cHSPは、HSPスクリプトの一部をネイティブCコードに変換し、コンパイル・実行することでスクリプトの実行速度を向上させるための拡張フロントエンドです。

独立したコンパイラフロントエンド（Linux: `chsp` CLI / Windows: `hspcmp.dll` Proxy DLL）として動作し、ネイティブトランスパイルから `hspcmp` への委譲までを透過的に実行します。

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
| `--compath=<path>` | 共通ディレクトリ (`common/`) のパスを指定 |
| `--chsp-compile=libtcc\|none` | ネイティブコンパイル方式を指定（既定値: `libtcc`） |
| `--keep-tmp` | 中間生成ファイル (`.tmp.hsp`, `.c`) を残置 |
| `--hspcmp=<path>` | 委譲先の `hspcmp` バイナリのパスを指定 |

### 3. テストの実行

```bash
make test-chsp
```

全テストマトリクス（通常モード、Cエミットモード、libtccネイティブ実行モード）が実行されます。

---

## 基本仕様と構文

詳細な仕様および対応命令一覧は [docs/chsp.md](../hspcmp/docs/chsp.md) を参照してください。

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

- [docs/chsp.md](../hspcmp/docs/chsp.md): cHSP の言語仕様・文法・対応構文リファレンス
- [docs/chsp-internals.md](../hspcmp/docs/chsp-internals.md): コンパイラ内部構造・AST エミッター・バックエンド設計
