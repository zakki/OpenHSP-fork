# SDL入力の回帰テスト

```sh
make -C test sdl-input
```

Linuxホスト上でSDLのdummyビデオドライバを使い、実際の`SDL_PushEvent`から
Linux / Emscriptenの`handleEvent`を実行する。通常Dish / HGIMG4それぞれの
実際の座標変換とBmscrのマルチタッチ管理を使い、4組×3ケースを検証する。
SDL2、SDL2_image/ttfのヘッダ、OpenGLの開発環境が必要。

- `keys`：A、Enter、矢印、F1、テンキーの数字・演算子・Enter、Shift、リピート、キー解放、割り込み無効時、既存GUI通知・キー状態の維持。
- `touch`：表示サイズと論理サイズが異なる場合、余白と拡大率、2本の指の移動・独立した解除。
- `view`：上記にビュー変換も加え、座標変換の順序を確認。

修正前はLinuxの3ケース、Emscriptenのkeys/viewが失敗した。
Emscriptenのビュー変換なしのtouchは修正前から成功。
修正後はHGIMG4を含め12ケースすべて成功。

## HSPとの関係

2-3はHSPの`onkey`登録先へ送る`HSPIRQ_ONKEY`の通知欠落。
2-4はHSPがmousex/mouseyやmtlist/mtinfoで観測するBmscrの座標と指情報の問題。
どちらもHSPに影響する経路だが、このテストはHSPスクリプトの割り込み先までは実行せず、
`code_sendirq`を記録用の関数に置き換えて通知を検証する。GUIコントロール処理、画面破棄、
ブラウザAPIもテスト対象外。Emscriptenの`stubs/`は未使用のブラウザAPIの宣言だけを提供する。

onkeyのwparamはWindows仮想キー番号に変換し、iparamはシフト適用前の文字コード
（英字は大文字、非文字キーは0）を渡す。lparamはWindowsメッセージそのものではない。
下位16bitは1、16..24bitはSDL scancode、bit30はリピート時に1とする。
SDL_TEXTINPUT/IME入力は従来のGUI通知経路のまま。

ブラウザ、実タッチ端末、実GPUでの操作は別途確認が必要。
