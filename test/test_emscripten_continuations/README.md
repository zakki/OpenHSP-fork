# Emscripten continuation 回帰テスト

対象コミット: `cf7f9034b`、`c6025b028`。

## 実行

64-bit Linux、C++17対応のg++、Python 3、ビルド済みのhspcmpを使用する。
リポジトリのルートから:

```sh
make hspcmp
make -C test emscripten-continuations
```

コンパイラを指定する場合:

```sh
make -C test/test_emscripten_continuations test HSPCMP=/absolute/path/to/hspcmp
```

`.ax` は一時ディレクトリに作成して削除する。ハーネスは `build/` に作成する。
実行時間制限と命令数制限により、ループや終了処理の回帰もテスト失敗になる。

## 保護する動作

- `nested_returns.hsp`: defcfunc → deffunc → deffunc と gosub の復帰、再帰、local変数、整数・文字列・実数の戻り値、式の続き、繰り返し呼び出し。
- `command_loop.hsp`: 入れ子のdeffunc内のループをルート実行器で分割実行すること、awaitからの継続、stat、ループと呼び出し階層の復元。同一ステップでループが完走してしまう場合も検出する。
- `callbacks.hsp`: 複数callbackのFIFO順序、iparam/wparam/lparam/stat/strsize/refstr/refdvalの保存、deffunc/gosubから復帰してもcallback_flagを保持すること、最後に呼び出し元へ戻ること。
- `calls.hsp`: code_callのFIFO順序と引数保存、callback_flag=0でのwait許可。
- `invalid_callback.hsp`: callback内のawaitをエラー42にすること。
- `on_gosub.hsp`: 選択されたラベルの同期呼び出し、入れ子、wait後の復帰、範囲外インデックスの無操作。
- `on_gosub_function.hsp`: defcfunc内のon gosubと関数呼び出し後の式の継続。
- `on_gosub_callback.hsp`: callback内のon gosubの実行順序とcallback_flagの保持。
- 正常終了時はsublev、looplev、評価スタック、保留callback、callback_flagが残っていないことを確認する。その後、実行中・待機中のcallbackがある状態でHsp3::Resetし、継続状態が消去されることも確認する。
- `redraw.cpp`: redraw 0/2の描画開始→BG→NORMAL、redraw 1/3のPOSTEFF→オブジェクト→MAX→描画完了、フォント復元、メイン画面だけのsprite更新、アイドル時の無操作。

回帰検出の確認として、一時コピーで `cf7f9034b` の復帰階層チェックを削除すると `nested_returns` がエラー32で失敗し、`cmdfunc_custom` を同期実行に戻すと `command_loop` が分割実行チェックで失敗することを確認した。

## 検証範囲

`runner.cpp` は実際のインタプリタを **HSPEMSCRIPTEN付きでホスト上にビルド**し、`code_execcmd_one` を実行する。テスト命令がネイティブ側からcode_call/code_callbackを呼ぶ。ファイルI/OにはLinux実装を使用し、wait/awaitは経過時間を待たず再開する。

`redraw.cpp` は本体の `hsp3gr_dish.cpp` をincludeし、非公開の描画開始関数も検証する。描画先のBmscr/hgio/spriteメソッドを記録用の偽物に置き換え、未使用のグラフィックス関数はリンク時に除外する。継続処理自体のコピーは作らない。

ブラウザ/Wasm、実際の描画結果、Emscriptenのフレームスケジューラと描画callbackの結合は、このテストでは検証しない。PlatformWindows.cppのHSPUTF8定義ガードもWindowsビルド未検証。

`on gosub`の3ケースは実際のHSPスクリプトを実行する。修正前のHEAD
`1edbe08c`ではそれぞれエラー10、40、5で失敗した。通常のルートからの単一呼び出しは
キューが次の命令より先に処理されるため、順序逆転しない。入れ子・同期関数内・callback内が
問題になる。`code_getlb2()`は次の命令を先読みしているため、修正時の復帰先は
通常のgosubの`mcs`ではなく`mcsbak`でなければならない。

## 調査で見つかった未対応経路

本体の実装は変更していない。以下は正常動作の回帰テストとは分けた再現ケースで、修正されるまでは失敗する。

```sh
make -C test/test_emscripten_continuations known-gaps
```

1. **同期defcfunc内からのcallback登録** (`known_gaps/synchronous_callback.hsp`)
   - 期待: callbackが各1回実行され、関数が7を返して呼び出し元が完了する。
   - 実測: `ERROR 40`（関数の戻り値なし）。
   - `src/hsp3/hsp3code.cpp:2948` のcode_callbackは `mcs = mcsbak` としてからキューに登録する。ルート側は継続を開始してPCを設定し直すが、`:1589` のcode_callfuncの同期ループはキューを処理せず、巻き戻されたPCで次の命令を処理する。同期関数内でlayerobj/objprm等がcallbackを登録する経路も点検が必要。

2. **callback内からの追加callback登録** (`known_gaps/nested_callback.hsp`)
   - 期待: outerが2回、innerが4回実行され、呼び出し元が1回完了する。再現テストは内外の実行順序までは固定しない。
   - 実測: 呼び出し元まで正常に戻らず `ERROR 10`（gosubのないreturn）。
   - `src/hsp3/hsp3code.cpp:1416` はcallback実行中にキューを処理しない。この経路もcode_callbackがPCを巻き戻したまま実行を続ける。加えて、`:1397` で登録時のreturn_pcを保存し、`:1502` で後から使用するため、親callbackのフレームを破棄した後に親の内部へ戻りうる。PCと親フレームの寿命を含めた対応が必要。

これらはテスト用ネイティブ命令から公開callback APIを呼ぶ最小再現であり、ブラウザ上のlayerobj/objprmの再現確認は別途必要。
