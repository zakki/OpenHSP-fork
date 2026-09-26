# HSP 3.7 Win32 配布パッケージ

Python 3.10以降の標準ライブラリだけで、公式HSP 3.7への追加ZIPを作成します。
ビルドは別工程です。`src/chsp/vsbuild.bat`等で現在のソースをWin32 Releaseとして
ビルドし、`src/chsp/Release/`に`chsp.exe`、`hspcmp.dll`、`libtcc.dll`を用意してください。

リポジトリルートで実行:

```powershell
python src/chsp/tools/package_win32.py
```

入力の既定値:

- 公式パッケージ: `dist/hsp37.zip`
- ビルド成果物: `src/chsp/Release/`
- TCCランタイム: `src/chsp/extlib/tcc/`
- 配布用原稿: `src/chsp/package/`
- ヘッダー: `common/chsp/`

出力は`dist/chsp_hsp37_win32.zip`です。ZIP最上位の`hsp37/`を公式ZIPと同じ場所へ
展開します。公式ファイルと衝突する収録内容はエラーにします。
`hspcmp.dll`は`hspcmp_chsp.dll`へ改名して収録し、公式コンパイラは転載しません。

引数で`--release-dir`、`--tcc-dir`、`--official-zip`、`--output`を変更できます。
相対パス引数は起動時の作業ディレクトリを基準とし、既定値はスクリプトの位置を基準にします。
同じ入力から同じZIPを生成するため、収録順とZIPタイムスタンプは固定しています。
入力検査とZIP検証が終わるまでは既存出力を置き換えません。

配布専用の利用者向け文書`src/chsp/package/chsp.md`を、バイト列を保って
`doclib/chsp.txt`にコピーします。`src/chsp/README.md`と`src/chsp/docs/`の2文書は
収録しません。利用時に必要な構文・手順・制限は配布専用文書にまとめます。
既存の開発用文書との重複整理は別作業とします。Markdown変換やリンク書き換えは行いません。
ヘルプと配布READMEはUTF-8原稿をCP932/CRLFへ変換します。変換不能文字はエラーです。
切替バッチと補助PowerShellはASCII/CRLFで収録します。

`sample/ao_opt.chsp`を`sample/chsp/ao_opt.hsp`として収録します。
ソースツリーの古い生成済み`sample/ao_opt.hsp`は使いません。
比較サンプルは`test/test_chsp_compare/generate_templates.py`を利用し、
スクリプト内の`SAMPLES`に列挙したテンプレートから各3形式を生成します。
テストの全件や生成済みバイナリは収録しません。

`chsp-package.json`には全収録ファイル（manifest自身以外）と公式DLLのSHA256を記録します。
切替処理はその値を検証し、未知のDLLと既存の不正なバックアップを拒否します。
更新前には旧パッケージの`disable_chsp.bat`で公式版へ戻してください。
過去のdelegateパッケージからの自動移行は行いません。

TCCのライセンス本文は旧配布物の`docs/TCC_COPYING`を`package/TCC_COPYING`へ移して
管理します。TCCを更新する際は実際に同梱するランタイムのライセンスも確認してください。

検証:

```powershell
python -m unittest discover -s src/chsp/tools -p "test_package_win32.py"
```

このテストは現在の配布入力と公式ZIPを使用します。Windowsでは隔離した一時フォルダーで
有効化・復元・繰り返し・不明DLL・バックアップ破損・使用中ファイルも検査します。
配布前には公式ZIPに重ねた環境でエディタのF5、ヘルプ検索、CLIの通常スクリプトと
chspスクリプトの実行も確認してください。通常の利用環境では切替テストを行いません。
