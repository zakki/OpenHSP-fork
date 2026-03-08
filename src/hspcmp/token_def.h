//
//	token.cpp structures
//
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

// token type
#define TK_NONE 0
#define TK_OBJ 1
#define TK_STRING 2
#define TK_DNUM 3
#define TK_NUM 4
#define TK_CODE 6
#define TK_LABEL 7
#define TK_VOID 0x1000
#define TK_SEPARATE 0x1001
#define TK_EOL 0x1002
#define TK_EOF 0x1003
#define TK_ERROR ( -1 )
#define TK_CALCERROR ( -2 )
#define TK_CALCSTOP ( -3 )

#define DUMPMODE_RESCMD 3
#define DUMPMODE_DLLCMD 4
#define DUMPMODE_ALL 15

#define CMPMODE_PPOUT 1
#define CMPMODE_OPTCODE 2
#define CMPMODE_CASE 4
#define CMPMODE_OPTINFO 8
#define CMPMODE_PUTVARS 16
#define CMPMODE_VARINIT 32
#define CMPMODE_OPTPRM 64
#define CMPMODE_SKIPJPSPC 128
#define CMPMODE_UTF8OUT 256

// line mode type
#define LMODE_ON 0
#define LMODE_STR 1
#define LMODE_COMMENT 2
#define LMODE_OFF 3

// macro default data storage
using MACDEF = struct MACDEF
{
	int index[32]; // offset to data
	char data[1];
};

// module related define
#define OBJNAME_MAX 60
#define MODNAME_MAX 20

#define COMP_MODE_DEBUG 1
#define COMP_MODE_DEBUGWIN 2
#define COMP_MODE_UTF8 4
#define COMP_MODE_STRMAP 8
#define COMP_MODE_LABOUT 16
#define COMP_MODE_SKIPERROR 64

#define SWSTACK_MAX 32

#define HEDINFO_RUNTIME 0x1000	   // 動的ランタイムを有効にする
#define HEDINFO_NOMMTIMER 0x2000   // マルチメディアタイマーを無効にする
#define HEDINFO_NOGDIP 0x4000	   // GDI+による描画を無効にする
#define HEDINFO_FLOAT32 0x8000	   // 実数を32bit floatとして処理する
#define HEDINFO_ORGRND 0x10000	   // 標準の乱数発生を使用する
#define HEDINFO_UTF8 0x20000	   // UTF8ランタイムを使用する(コード識別用)
#define HEDINFO_HSP64 0x40000	   // 64bitランタイムを使用する(コード識別用)
#define HEDINFO_IORESUME 0x80000   // ファイルI/Oエラーを無視して処理を続行する
#define HEDINFO_AUTOTIMER 0x100000 // マルチメディアタイマーを強制的に設定にする

enum ppresult_t
{
	PPRESULT_SUCCESS,			// 成功
	PPRESULT_ERROR,				// エラー
	PPRESULT_UNKNOWN_DIRECTIVE, // 不明なプリプロセッサ命令（PreprocessNM）
	PPRESULT_INCLUDED,			// #include された
	PPRESULT_WROTE_LINE,		// 1行書き込まれた
	PPRESULT_WROTE_LINES,		// 2行以上書き込まれた
};

#define LABBUF_FLAG_NONE ( 0 )
#define LABBUF_FLAG_FUNC ( 1 )
#define LABBUF_FLAG_MACRO ( 2 )
#define LABBUF_FLAG_LABEL ( 3 )
#define LABBUF_FLAG_VAR ( 4 )
#define LABBUF_FLAG_EXVAR ( 5 )
#define LABBUF_FLAG_CMD ( 6 )
#define LABBUF_FLAG_EXCMD ( 7 )
#define LABBUF_FLAG_REFER ( 0x100 )

#define LABLIST_MODE_LABEL ( 0 )
#define LABLIST_MODE_VAR ( 1 )
#define LABLIST_MODE_ALL ( 2 )
#define LABLIST_MODE_REFERENCE ( 16 )
#define LABLIST_MODE_PARTMATCH ( 32 )
