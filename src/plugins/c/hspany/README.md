# hspany

`hspany` は、配列要素ごとに `int`、`str`、`double`、`int64`、ラベルを保持できる HSP3 変数型プラグインです。

## 使い方

```hsp
#include "hspany.as"

dimtype v, vartype("any"), 5
v(0) = 123
v(1) = "abc"
v(2) = 1.5
v(3) = int64("8589934592")
v(4) = *handler

mes v(0)
mes v(1)
gosub v(4)
```

`hspany.as` は `#regcmd "hsp3cmdinit", ..., 1` で拡張変数型スロットを 1 つ確保してから、プラグイン初期化時に `registvar(-1, HspVarAny_Init)` で `any` 型を登録します。

## Linux build

```sh
cd src/plugins/c/hspany
make
../../../../hspcmp -d -i -u --compath=../../../../common/ sample_any.hsp
../../../../hsp3cl sample_any.ax
../../../../hspcmp -d -i -u --compath=../../../../common/ sample_oop.hsp
../../../../hsp3cl sample_oop.ax
```

Linux では `hspany.as` が `./hspany.so` をロードします。実行時のカレントディレクトリに `hspany.so` がある状態で使ってください。

## Windows build

Visual Studio 2022 で `hspany.sln` を開いてビルドします。32bit 版は `hspany.dll`、64bit 版は `hspany_64.dll` を出力します。`hspany.as` は `__hsp64__` でロードする DLL 名を切り替えます。

## 制限

これはコア未変更で動かすプラグイン実装なので、通常の型と完全に同じ振る舞いにはなりません。

- `dimtype v, vartype("any"), n` のように明示的に `any` で確保してください。
- `v(0) = 1`、`v(1) = "x"` のような明示インデックス代入と参照が主対象です。
- `v(2) = *label` のようにラベルを保持し、`gosub v(2)` のようなラベル引数として渡せます。
- `v = 1` のようなスカラー代入は、HSP コアの `cmdfunc_var()` により変数自体が代入元の型へ変わる場合があります。
- `v(0) = 1, "x", 2.5` のような複数要素代入は対象外です。
- 他の組み込み命令や別プラグインが `HspFunc_prm_setva` 経由で `any` へ書き込む場合は、コア側 `code_setva()` の対応がない限り正しく扱えません。
- コア組み込み版の `any` と同時に使うと型名が重複します。プラグイン版は、コアに `any` が組み込まれていないランタイムで使う想定です。
