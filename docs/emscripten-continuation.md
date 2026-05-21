# Emscripten continuation maintenance notes

この文書は、Emscripten 版 HSP3/HSP3Dish の no-Asyncify 実行ループで、ネイティブ版のネスト実行に近い順序を保つための保守メモです。実装計画の詳細はソースコメントとテストに移し、ここでは変更時に確認すべき契約だけを残します。

## 目的

Emscripten 版ではブラウザまたは Node.js のメインループから `code_execcmd_one()` を少しずつ進めます。そのため、ネイティブ版のように C++ の呼び出しスタック上で `gosub`、`code_call()`、`code_callback()`、command-style `#deffunc` が完了するとは限りません。

この差を吸収するため、`HSPEMSCRIPTEN` では HSP 側の継続を root executor が処理する明示的なフレームとして管理します。Asyncify は前提にしていません。

## 実装上の契約

- `code_execcmd_one()` は通常命令より前に pending HSP continuation を進めます。
- `code_call()` と `code_callback()` は Emscripten では即時ネスト実行せず、FIFO の callback frame として enqueue します。
- callback frame は `iparam`、`wparam`、`lparam`、`stat`、`strsize`、`refstr`、`refdval`、`callback_flag` を snapshot します。`reffunc_sysvar()` に callback 可視のシステム変数を追加する場合は、この snapshot 対象も見直してください。
- command-style `#deffunc` は既存の custom function stack frame を使い、`cmdfunc_return()` で unwind します。
- expression-style `#defcfunc` は呼び出し元が戻り値を同期的に必要とするため、この no-Asyncify continuation の対象外です。
- callback 実行中の `wait` / `await` はネイティブ版と同じく callback 内不正動作として扱います。

## HSP3Dish redraw

HSP3Dish の layer callback は HSP continuation を enqueue できます。`redraw` は Emscripten で native continuation phase に分割し、active または queued HSP continuation がある間は native phase を進めません。

`redraw 1` の順序は POSTEFF callback、`DrawAllObjects()` / `SetDefaultFont()`、MAX callback、`hgio_redraw()` です。`redraw 0` は `hgio_redraw()` の後に BG/NORMAL callback を処理します。

## ビルドターゲット

`makefile.emscripten` には通常のブラウザ向け `hsp3dish.js` / `hsp3dish-gp.js` に加えて、比較テスト用の明示ターゲットを置いています。これらは default `TARGETS` には含めません。

- `hspcmp.js`: Emscripten 版 compiler
- `hsp3cl.js`: 通常 CLI runtime
- `hsp3cl_loop.js`: Node.js main-loop runtime
- `hsp3cl_loop_test.js`: `HSP3_CORE_TEST` 付き比較テスト runtime

`HSP3_CORE_TEST` 用 extcmd は `hsp3cl_loop_test.js` 専用です。通常 runtime や HSP3Dish にリンクしないでください。

## 検証

代表的な確認コマンドは次の通りです。

```sh
emmake make -f makefile.emscripten hsp3cl_loop_test.js hspcmp.js
make -f makefile hsp3cl_coretest
make -C test/test_emscripten_core_compare compare MODE=all NATIVE_HSPCMP=../../hspcmp NODE=node
emmake make -f makefile.emscripten hsp3dish.js hsp3dish-gp.js
```

ブラウザ描画の最終確認が必要な場合は、layerobj callback を使う redraw サンプルと Reversi などの `await` を含む HSP3Dish サンプルを手動で確認してください。
