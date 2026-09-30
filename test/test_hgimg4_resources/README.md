# HGIMG4 リソース管理の回帰テスト

リポジトリのルートで実行する。

```sh
make hspcmp
make -C test hgimg4-resources
```

Linux x64、Python 3、ASan/LSan対応のg++と、通常の`hsp3gp`ビルド依存ライブラリが必要。
ウィンドウやGPUは作らない。`gamehsp`、`gpmat`、GamePlayのModel・Material・Node・Bundle・Imageを
実際に使用する。モデルの初期配置だけはテスト用に非公開メンバへアクセスする。
対象のC++ファイルをASan付きでコンパイルし、他の依存オブジェクトは通常のビルドを利用する。
実行ログは`build/<ケース名>.log`に出力する。

## HSPからの到達性

C++で再現することと、公開HSP命令から到達できることを区別する。
`scripts/`はHSPコンパイラでコンパイルして実際の命令ディスパッチャに渡す。
描画の初期化は省略し、オブジェクト0にヘッドレスのModelを配置する。
`check`と`inspect_matrices`だけがテスト用の検証命令。

| 指摘 | 到達性 | テスト・確認内容 |
| --- | --- | --- |
| 1-1 プロキシの参照不足 | HSPから到達 | `gpnodeinfo ..., GPNODEINFO_MATERIAL` → `delobj` → `gpmatprm1`。`scripts/object_first.hsp`、`proxy_first.hsp`で両削除順序を検証。公開ヘッダの削除命令名は`delobj`。 |
| 1-2 存在しないノード名 | C++直接呼び出しの問題 | `makeNewMatFromObj`の直接呼び出しはクラッシュするが、HSPの`gpnodeinfo`が通る`getNodeInfo`は事前にNULLを判定する。`scripts/missing_node.hsp`は修正前から成功する保護テスト。 |
| 1-2 非ModelのDrawable | C++側の防御 | 非ModelのDrawableをテストで配置して失敗を確認。通常のHSP命令による該当Drawableの生成経路は実証していない。 |
| 1-3 マスク取得 | C++ APIの問題 | `getPixelMaskBuffer`を直接呼ぶ。`Bmscr::getPixelMaskBuffer` → `hgio_texmaskbuffer`の経路はあるが、現行ソースでBmscrメソッドを呼ぶHSP命令は見つからない。 |
| マスクのサイズ判定 | C++ APIの問題 | `hgio_texmaskbuffer`に2×2画像を渡し、両辺一致・幅だけ一致・高さだけ一致・両辺不一致を検証。両辺一致の場合だけバッファを返す。 |
| 1-4 読み込み失敗 | HSPから到達 | `gpload`が`makeModelNode`を直接呼ぶ。ノードなし、シーンなし、materialなし、bundleなしを個別のC++ケースと`load_errors.hsp`で検証。 |
| 1-5 行列配列 | HSPから到達 | `gpmatprm16`はマテリアルIDなら複数行列を受け付ける。`matrices.hsp`で値、別パラメータへの再設定、プロキシ削除後の生存を検証。オブジェクトIDに対してはHSP命令側がcount=1に制限する。 |
| シーン終了時の解放 | HSPの終了・gpreset経路 | `deleteAll`がMATFLAGなしのIDで`deleteMat`を呼び、全マテリアルを取りこぼしていた。C++の`reset`ケースでリークを検出。描画初期化を伴うHSPのgpresetそのものは実行しない。 |
| 3 x64メンバアクセス | HSPのgpsetprm/gpgetprm経路 | C++テストで全15定数、ビット操作、FLAGの隣接メンバ保護、不正IDを検証。修正前から成功し、本体の変更なし。 |

## 修正前後の結果

修正前HEAD `1edbe08c`の`gamehsp.cpp`・`gpmat.cpp`を別オブジェクトとしてリンクし、
同じテストハーネスでも確認した。21ケース中、`x64`と`hsp_missing_node`の2件だけが成功。
修正後は21件すべて成功。

サイズ判定の追加4ケースでは、修正前の`||`で`mask_size_width_only`と
`mask_size_height_only`が失敗し、`&&`への修正後は両方とも成功する。

- `object_first` / `proxy_first`と対応するHSPスクリプト：ASanでheap-use-after-free。
- `texture`：プロキシ削除時に共有マテリアルのパラメータが消える。
- `missing_node` / `non_model`：NULL参照でクラッシュ。
- `mask`：画像の読み込みには成功するがNULLが返る。修正後はImageの下から上への行順を維持した4画素のα値を確認。
- `load_node` / `load_scene` / `load_material`：LSanで448 / 920 / 272バイトのリーク。
- `load_bundle`：NULLのBundle参照。HSPの`load_errors`もこの経路でクラッシュ。
- `matrix_leak`：削除後にバッファが残る。`matrix_values` / `hsp_matrices`：2番目の行列が不一致。
- `matrix_names`：別のuniformを設定すると以前の配列が解放され、ASanでUAF。
- `reset`：所有するMaterialの176バイトのリーク。

共有Materialが生き残るため、単に`revoke`を追加するだけでは不十分。
配列用uniformは`setMatrixArray(..., true)`でパラメータ自身にコピーを保持させる。

## 検証範囲

描画結果、GPUリソース、ブラウザでの実行は検証しない。
LSanは実際の検査ができる環境で実行する。ptrace制限のあるサンドボックスでは失敗する場合がある。
リンクされたランタイムのプロセス起動時のキャッシュとHSPバイトコード読み込みはLSan追跡の対象外。
HSPの読み込みには既存の`Hsp3::copy_DAT`由来の解放漏れがあり、この修正の対象に含めない。
フィクスチャの生成と、HSP命令を含む対象操作の実行中は追跡を有効にしている。
