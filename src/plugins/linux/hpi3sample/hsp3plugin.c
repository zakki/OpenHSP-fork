//
//		HSP3.0 Plugin SDK main
//		onion software/onitama 2004/9
//

#include <stdio.h>
#include <stdlib.h>

#include "hsp3plugin.h"

/*------------------------------------------------------------*/
/*
		system data
*/
/*------------------------------------------------------------*/

int p1,p2,p3,p4,p5,p6;
int *type;
int *val;
PVal *mpval;		// Master PVal pointer
HSPCTX *ctx;		// Current Context
HSPEXINFO *exinfo;	// Info for Plugins


void hsp3sdk_init( HSP3TYPEINFO *info )
{
	//		SDK初期化
	//
	ctx = info->hspctx;
	exinfo = info->hspexinfo;
	type = exinfo->nptype;
	val = exinfo->npval;
}


int code_getprm( void )
{
	//		パラメーターを取得(型は問わない)
	//
	int res;
	res = exinfo->HspFunc_prm_get();
	mpval = *exinfo->mpval;
	return res;
}
