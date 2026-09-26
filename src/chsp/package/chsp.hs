; cHSP help source. Maintained in UTF-8; packaged as CP932/CRLF.
%type
cHSP拡張ディレクティブ
%ver
3.7
%note
cHSP追加パッケージ (Win32)
%date
2026/09/26
%author
OpenHSP contributors
%port
Win

%index
#chsp_module
ネイティブ化するモジュールを開始する
%group
cHSPディレクティブ
%prm
"name" target=plugin
"name" : 出力するネイティブライブラリ名 (省略可能)
target : plugin または c (省略時はplugin)
%inst
#chsp_module_endまでをネイティブ化の対象モジュールとします。
モジュール内に#chsp_deffuncや#chsp_defcfuncで関数を定義します。
nameを省略すると、ソースファイル名とモジュールの出現順から出力名を生成します。
target=pluginはHSP3プラグイン形式、target=cは通常のC関数を公開する形式です。
1ファイルに複数のモジュールを記述でき、それぞれ独立したCソースとDLLを生成します。
出力名と公開関数名が重複しないようにしてください。
^
HSPエディタから使用するにはenable_chsp.batでchsp版コンパイラを有効にします。
変換する定義は入口ファイルへ直接記述してください。#include先のchsp定義は変換されません。
分割する場合はchsp.exe --libraryでinclude用ライブラリを先に生成します。
詳細はdoclib/chsp.txtを参照してください。
%sample
#chsp_module "answer_native" target=plugin
#chsp_defcfunc int answer int p_value
    return p_value + 1
#chsp_end
#chsp_module_end
mes answer(41)
stop
%href
#chsp_module_end
#chsp_deffunc
#chsp_defcfunc

%index
#chsp_module_end
ネイティブ化するモジュールを終了する
%group
cHSPディレクティブ
%prm
%inst
#chsp_moduleで開始したモジュールを終了します。
関数定義は先に#chsp_endで終了してください。
以降には通常のHSPコードや別のchspモジュールを記述できます。
%sample
#chsp_module "empty_native"
#chsp_module_end
%href
#chsp_module
#chsp_end

%index
#chsp_deffunc
戻り値のないネイティブ命令を定義する
%group
cHSPディレクティブ
%prm
name 型 引数名, ...
name : 公開する命令名
型 : int、double、array[int]、array[double]など
%inst
モジュール内で戻り値のない命令を定義します。定義の終わりに#chsp_endを記述します。
引数には型を指定します。ローカル変数は引数リストにlocal[int]やlocal[double]で宣言します。
固定長ローカル配列はlocal[int[n]]、local[double[n]]で宣言します。
引数配列は最大4次元のアクセス、ローカル配列は1次元に対応します。
^
通常のHSPグローバル変数を直接参照できません。必要な値・配列を引数で渡してください。
通常のHSPユーザー関数やGUI命令は呼び出せません。
関数内では代入、if/else、repeat/loop、break、continue、returnなどを使用できます。
%sample
#chsp_module "fill_native"
#chsp_deffunc fill array[int] p_values, int p_value
    p_values(0) = p_value
    return
#chsp_end
#chsp_module_end
dim values, 1
fill values, 42
mes values(0)
stop
%href
#chsp_module
#chsp_defcfunc
#chsp_end

%index
#chsp_defcfunc
戻り値のあるネイティブ関数を定義する
%group
cHSPディレクティブ
%prm
戻り値型 name 型 引数名, ...
戻り値型 : int または double
name : 公開する関数名
%inst
モジュール内で数値を返す関数を定義します。returnで値を返し、#chsp_endで定義を終了します。
引数とローカル変数の型指定は#chsp_deffuncと同じです。
関数は式の中で呼び出します。文字列型の戻り値や引数には対応していません。
intとdoubleの混在演算・暗黙変換には制限があります。詳細はdoclib/chsp.txtを参照してください。
%sample
#chsp_module "square_native"
#chsp_defcfunc double square double p_value
    return p_value * p_value
#chsp_end
#chsp_module_end
mes square(2.5)
stop
%href
#chsp_deffunc
#chsp_end
#chsp_module

%index
#chsp_end
ネイティブ関数の定義を終了する
%group
cHSPディレクティブ
%prm
%inst
#chsp_deffuncまたは#chsp_defcfuncの定義を終了します。
実行時に関数から戻るreturnとは役割が異なります。
モジュール全体の終了には#chsp_module_endを使用します。
%sample
#chsp_module "identity_native"
#chsp_defcfunc int identity int p_value
    return p_value
#chsp_end
#chsp_module_end
%href
#chsp_deffunc
#chsp_defcfunc
#chsp_module_end

%index
#chsp_c
モジュールへCコードを埋め込む
%group
cHSPディレクティブ
%prm
{"Cコード"}
%inst
モジュール内へCコードをそのまま埋め込みます。複数行で記述できます。
モジュールの外では使用できません。target=pluginとtarget=cの両方で使用できます。
埋め込んだC関数をchsp関数から呼ぶには#chsp_cdeclで関数名を宣言します。
追加のリンクライブラリが必要な場合は#chsp_clinkも指定してください。
%sample
#chsp_module "helper_native"
#chsp_c {"
static int plus_one(int value) {
    return value + 1;
}
"}
#chsp_cdecl plus_one
#chsp_defcfunc int answer int p_value
    return plus_one(p_value)
#chsp_end
#chsp_module_end
mes answer(41)
stop
%href
#chsp_cdecl
#chsp_clink
#chsp_module

%index
#chsp_cdecl
埋め込んだC関数を呼び出し可能にする
%group
cHSPディレクティブ
%prm
name
name : C関数名
%inst
#chsp_cで記述したC関数の名前を登録し、chsp関数内から呼び出せるようにします。
モジュール内かつchsp関数本体の外に記述します。
登録していないC関数名はchsp関数から呼び出せません。
使用例は#chsp_cを参照してください。
%href
#chsp_c
#chsp_module

%index
#chsp_clink
モジュールに追加ライブラリをリンクする
%group
cHSPディレクティブ
%prm
"name"
"name" : 追加リンクするライブラリ名
%inst
現在のモジュールのDLL生成時に追加でリンクするライブラリを指定します。
モジュール内に1行につき1つ指定し、複数必要な場合は複数行記述します。
target=pluginとtarget=cの両方で使用できます。
対象環境のTCCから参照できるライブラリを指定してください。
Linux専用ライブラリの指定をWindowsでそのまま使用することはできません。
%sample
#chsp_module "native_math" target=c
#chsp_clink "msvcrt"
#chsp_defcfunc double root double p_value
    return sqrt(p_value)
#chsp_end
#chsp_module_end
%href
#chsp_c
#chsp_module
