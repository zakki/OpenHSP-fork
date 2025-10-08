# Raspberry Pi Pico 対応開発ガイド

## 開発環境

- Windows上で [Visual Studio Code](https://code.visualstudio.com/) と [Raspberry Pi Pico Extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico) を使用しています。
- 動作確認は Raspberry Pi Pico W で行っています（WiFi機能は未使用）。PicoやPico 2でも動作する可能性があります。
- Freenove Starter Kit for Raspberry Pi Pico W を利用しています。

## ビルド手順

1. VS Code 左側の「Raspberry Pi Pico Project」タブから `Project` → `Compile Project` を選択し、OpenHSPをビルドします。
2. `build/PicoHSP.elf` が生成されます。
3. Raspberry Pi Picoの「BOOTSEL」ボタンを押しながらPCにUSB接続します。
4. WindowsのエクスプローラーでPicoのフォルダが表示されます。
5. VS Codeで `Run Project(USB)` を実行すると、PicoHSPがラズパイに書き込まれます。
6. 書き込み後、自動的にPicoHSPが実行されます。

## HSPプログラムの実行方法

1. HSPエディタで `sample/rpipico/gpio1.hsp` を開き、`start.ax` を作成します。
2. GPIO 15にLEDを接続するか、プログラム内の `devcontrol "gpio", 15, 1` などの命令をボードに合わせて変更します。
3. PicoHSPをPCとUSB接続すると、10秒間USBメモリとして認識されます。エクスプローラーで `start.ax` を書き込みます。
4. 10秒後に `start.ax` が実行され、LEDが点滅します。
5. シリアルモニタでCOMポートを監視すると、`mes` 出力が表示されます。

例:
```
Hello HSP 10
Hello HSP 11
```

## 今後の課題

- VS Codeに依存しないビルド方法の検討
- PicoHSP.elfの配布・書き込み手順の整理
- axファイルの書き込み方法の改善
- Pico W以外での動作検証
- ドキュメントのさらなる改善
