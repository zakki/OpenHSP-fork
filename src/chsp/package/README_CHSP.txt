cHSP / HSP 3.7 Win32 追加パッケージ
=================================

導入
----
1. 公式 hsp37.zip を展開します。
2. この ZIP を同じ場所へ展開し、hsp37 フォルダーを重ねます。
   hsp37 の中へさらに hsp37 を作らないでください。
3. HSPエディタをすべて終了し、hsp37/enable_chsp.bat を実行します。
4. エディタで sample/chsp/hello.hsp を開いて F5 を押します。
   42 と表示されれば導入できています。

ZIPの展開だけでは公式 hspcmp.dll は変更されません。
enable_chsp.bat は公式DLLを hspcmp_original.dll にリネームし、
hspcmp_chsp.dll を hspcmp.dll へコピーします。
元のDLLはchsp版からの委譲にも必要です。削除しないでください。
切替にはWindows PowerShell 5.1以降を使用します。
バッチの /quiet 引数で終了時のキー入力待ちを省略できます。

復元・更新
----------
元の公式版に戻すには、エディタ終了後に disable_chsp.bat を実行します。
hspcmp_original.dll は復元後も保持します。
更新は「旧パッケージで無効化 → 新ZIPを展開 → 有効化」の順です。
旧 delegate パッケージからは、公式ZIPを別の場所へ展開して導入してください。
実行中のエディタがある場合、終了させてから切替をやり直してください。

ドキュメント
------------
doclib/chsp.txt           利用方法・構文・制限
doclib/chsp-license/      ライセンス

hsphelp/chsp.hs に8つのディレクティブのヘルプを収録しています。
ヘルプビューアーを再起動して検索してください。

サンプル
--------
sample/chsp/hello.hsp       最小例
sample/chsp/ao_opt.hsp      高速化したaobench (元のao_opt.chsp)
sample/chsp/ao_original.hsp 比較用の通常HSP版
sample/chsp_test/           数値・配列・分岐の比較例

比較例の *_hsp.hsp は通常HSP、*_chsp_c.hsp はtarget=c、
*_chsp_p.hsp はtarget=pluginです。*.gtは期待するテキスト出力です。
コンパイルするとサンプルの隣にCソースやDLL、AXなどが生成されます。

CLI例 (hsp37から実行)
--------------------
chsp.exe --compath=common/ --hspcmp=hspcmp.exe -i -u sample/chsp/hello.hsp

別の作業フォルダーから実行する場合は、chsp.exe、--compath、
--hspcmpにインストール先の絶対パスを指定してください。
CLIの利用にDLL切替は不要です。
現在のCLIには、空白を含むパスで委譲先のhspcmp.exeを起動できない
制限があります。CLIを使う場合は、空白を含まない場所へ展開してください。

対象・制限
----------
HSP 3.7の32bit版向けです。64bitランタイム用DLLは生成しません。
int64などの64bit整数機能はHSP 3.8開発版向けであり、本パッケージでは使用できません。
高速化する定義は入口のhspファイルへ直接記述してください。
include先のchsp定義は自動変換されません。
分割する場合はchsp.exe --libraryで先にライブラリを生成します。
LIBTCC_DIRとHSPCMP_ORIGINALが設定されていると、それぞれ外部の
TCCランタイムと委譲先DLLが優先されます。通常は未設定で使用します。

chsp-package.json はファイルのSHA256と対応する公式DLLの情報です。
DLL切替にも使用するため、編集・削除しないでください。
