# HSP Win32 用 cHSP 配布パッケージ作成ツール

Python 3.10 以降の標準ライブラリのみを使用し、HSP Win32 環境へ追加可能な cHSP 配布パッケージ（ZIP）を生成するスクリプトです。

コンパイル成果物のビルド自体は別工程です。事前に `src/chsp/vsbuild.bat` 等で Win32 Release ビルドを実行し、`src/chsp/Release/` に `chsp.exe`、`hspcmp.dll`、`libtcc.dll` を用意してください。

## 実行方法

リポジトリルートで実行します:

```powershell
python src/chsp/tools/package_win32.py
```

### コマンドライン引数

| 引数 | 既定値 | 説明 |
| --- | --- | --- |
| `--release-dir` | `src/chsp/Release/` | ビルド成果物（`chsp.exe`, `hspcmp.dll`, `libtcc.dll`）が配置されたフォルダー |
| `--tcc-dir` | `src/chsp/extlib/tcc/` | TCC ランタイムフォルダー（ヘッダー、ライブラリ、ドキュメント） |
| `--official-zip` | `dist/hsp37.zip` | 検証用公式 HSP パッケージ ZIP（省略可能） |
| `--output` | `dist/chsp_hsp37_win32.zip` | 生成する配布パッケージ ZIP の出力パス |

- `--official-zip` を指定した（または既定のパスに存在する）場合、公式パッケージ内の保護対象ファイルとの上書き衝突検査を行い、公式 `hspcmp.dll` の SHA256 ハッシュを記録します。存在しない場合でも単体でパッケージ ZIP を生成可能です。
- 相対パス引数は実行時のカレントディレクトリを基準とし、既定値はスクリプトの位置（リポジトリルート）を基準とします。
- 収録順序および ZIP 内ファイルのタイムスタンプを固定しているため、同一入力からは常に同一ハッシュの ZIP が生成されます。
- 入力検査と ZIP 整合性検証が完了するまで、既存の出力ファイルを置き換えません。

## パッケージ構成

出力される ZIP の最上位は `hsp37/` ディレクトリとなっており、HSP インストールフォルダーにそのまま上書き展開して利用する構造です。公式コンパイラ自体は転載せず、公式ファイルと衝突する収録内容はエラーとして除外されます。

- **実行ファイル・DLL**:
  - `chsp.exe`: コマンドライン用 cHSP コンパイラ
  - `hspcmp_chsp.dll`: cHSP 対応コンパイラ DLL（切替スクリプトにより `hspcmp.dll` として配置）
  - `libtcc.dll`: TCC ランタイム DLL
- **ヘッダー・ライブラリ**:
  - `common/chsp/`: cHSP 用ヘッダーおよびテーブル（`chsp_runtime.h`, `chsp_builtins.tsv` 等）
  - `tcc/include/`, `tcc/lib/`: TCC ランタイムの C ヘッダーおよびライブラリ
- **ドキュメント・ライセンス**:
  - `README_CHSP.txt`: 配布用 README（CP932/CRLF）
  - `doclib/chsp.txt`: cHSP 利用ガイド（Markdown 原稿 `src/chsp/package/chsp.md` のテキスト）
  - `doclib/chsp-license/`: OpenHSP および TCC のライセンス文書
  - `hsphelp/chsp.hs`: HSP ディレクティブ用ヘルプ（CP932/CRLF）
- **切替スクリプト**:
  - `enable_chsp.bat`, `disable_chsp.bat`, `switch_chsp.ps1`: コンパイラ DLL の有効化・無効化バッチおよびスクリプト（ASCII/CRLF）
- **サンプル**:
  - `sample/chsp/`: `hello.hsp`, `ao_opt.hsp` 等のサンプル
  - `sample/chsp_test/`: 構文比較サンプル（`test/test_chsp_compare/` のテンプレートから生成された `hsp`, `chsp_c`, `chsp_p` 各形式）
- **パッケージマニフェスト**:
  - `chsp-package.json`: 収録ファイルの SHA256 ハッシュリスト（DLL 切替時の改ざん・破損検出に使用）

## コンパイラ DLL の切替機構

HSP スクリプトエディタはフォルダー内の `hspcmp.dll` を呼び出してコンパイルを行うため、本パッケージでは既存の環境を壊さずに切り替えられる安全機構を備えています。

- **有効化 (`enable_chsp.bat`)**:
  既存の `hspcmp.dll` を `hspcmp_original.dll` としてバックアップ退避し、`hspcmp_chsp.dll` を `hspcmp.dll` に配置します。配置前に `chsp-package.json` を照合し、`hspcmp_chsp.dll` の破損や改ざんがないことを確認します。既存のバックアップ（`hspcmp_original.dll`）が既に存在する場合は、元のコンパイラを保護するため上書きしません。
  現在のDLLが配布DLLとも既存バックアップとも異なる場合は、現在のDLLを失わないよう変更せず停止します。
- **無効化 (`disable_chsp.bat`)**:
  退避されていた `hspcmp_original.dll` を `hspcmp.dll` に復元し、元のコンパイラ状態に戻します。
  復元済みかどうかはバックアップとの一致で判定します。配布DLLやマニフェストを参照しないため、ZIPを更新した後や配布DLLの欠落・破損時も復元できます。バックアップがない場合は変更せずエラーにします。
- **対応環境**:
  元のコンパイラ DLL のハッシュ値を固定的に制限しないため、HSP 3.7 正式版だけでなく、開発版（HSP 3.8 等）やカスタム版の環境でも安全に退避・復元して利用可能です。

## テストと検証

以下のコマンドでテストを実行できます:

```powershell
python -m unittest discover -s src/chsp/tools -p "test_package_win32.py"
```

- パッケージ生成処理、収録ファイルの構成、マニフェストハッシュ、ZIP 生成の再現性をテストします。
- Windows 環境では、一時フォルダー上で以下のような DLL 切替シナリオの検証を実行します:
  - 有効化・無効化の繰り返しや冪等性
  - 任意バージョンの `hspcmp.dll` からのバックアップ退避と復元
  - 既存バックアップ保護と、異なる現在のDLLの消失防止
  - ペイロード（`hspcmp_chsp.dll`）破損・欠落時の有効化拒否とバックアップからの復元
  - 有効化したまま新ZIPを展開した後の復元・再有効化
  - ファイルロック（使用中）時の保護
- 公式パッケージ（`dist/hsp37.zip`）が存在する場合は、公式環境への重ね合わせ、CLI 実行、エディタ連携用 DLL インターフェース、ヘルプ検索の動作確認（スモークテスト）も自動実行されます。
