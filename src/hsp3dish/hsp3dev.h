
//
//	hsp3dev.h header (dish)
//
#ifndef __hsp3dev_h
#define __hsp3dev_h

#include <string>
#include "../hsp3/hsp3config.h"
#include "../hsp3/hsp3code.h"

typedef struct {
	//	デバイスごとの情報
	//	(*の項目は、親アプリケーションで設定されます)
	//
	char *devname;				// *デバイスランタイム名
	char *error;				// *エラーメッセージ

	//	ファンクション情報
	//
	int (*devprm)( char *name, char *value );	// パラメーター設定ファンクション
	int (*devcontrol)( char *cmd, int p1, int p2, int p3 );	// コマンド受け取りファンクション
	int *(*devinfoi)( char *name, int *size );	// int情報受け取りファンクション
	char *(*devinfo)( char *name );				// str情報受け取りファンクション

} HSP3DEVINFO;

#endif
