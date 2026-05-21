// Test-only HSP command hooks for Emscripten core continuation tests.
// This file is linked only into hsp3cl_coretest and hsp3cl_loop_test.js.
#include <stdio.h>
#include <string.h>

#include "../../src/hsp3/hsp3config.h"
#include "../../src/hsp3/hsp3code.h"
#include "../../src/hsp3/hsp3debug.h"
#include "../../src/hsp3/hsp3struct.h"

static HSPCTX *ctx;

#ifdef HSPEMSCRIPTEN
static unsigned short *coretest_phase_posteff;
static unsigned short *coretest_phase_max;
static int coretest_phase;

int hsp3coretest_has_native_continuation( void )
{
	return coretest_phase != 0;
}

int hsp3coretest_run_native_continuation_step( void )
{
	switch( coretest_phase ) {
	case 1:
		code_callback( coretest_phase_posteff );
		coretest_phase = 2;
		break;
	case 2:
		printf( "draw-all-objects\n" );
		coretest_phase = 3;
		break;
	case 3:
		code_callback( coretest_phase_max );
		coretest_phase = 4;
		break;
	default:
		printf( "hgio-redraw\n" );
		coretest_phase = 0;
		break;
	}

	return RUNMODE_RUN;
}
#endif

static int cmdfunc_coretest( int cmd )
{
	code_next();

	switch( cmd ) {
	case 0x00:
		{
		unsigned short *label1 = code_getlb2();
		int iparam1 = code_getdi( 1 );
		unsigned short *label2 = code_getlb2();
		int iparam2 = code_getdi( 2 );

		ctx->iparam = iparam1;
		ctx->wparam = iparam1 * 10;
		ctx->lparam = iparam1 * 100;
		code_callback( label1 );

		ctx->iparam = iparam2;
		ctx->wparam = iparam2 * 10;
		ctx->lparam = iparam2 * 100;
		code_callback( label2 );
		break;
		}
	case 0x01:
		{
		unsigned short *posteff = code_getlb2();
		unsigned short *max = code_getlb2();
#ifdef HSPEMSCRIPTEN
		coretest_phase_posteff = posteff;
		coretest_phase_max = max;
		coretest_phase = 1;
#else
		code_callback( posteff );
		printf( "draw-all-objects\n" );
		code_callback( max );
		printf( "hgio-redraw\n" );
#endif
		break;
		}
	case 0x02:
		{
		unsigned short *label1 = code_getlb2();
		unsigned short *label2 = code_getlb2();

		ctx->iparam = 11;
		ctx->wparam = 110;
		ctx->lparam = 1100;
		ctx->stat = 111;
		ctx->strsize = 1111;
		strncpy( ctx->refstr, "alpha", HSPCTX_REFSTR_MAX - 1 );
		ctx->refstr[HSPCTX_REFSTR_MAX - 1] = 0;
		ctx->refdval = 1.25;
		code_callback( label1 );

		ctx->iparam = 22;
		ctx->wparam = 220;
		ctx->lparam = 2200;
		ctx->stat = 222;
		ctx->strsize = 2222;
		strncpy( ctx->refstr, "beta", HSPCTX_REFSTR_MAX - 1 );
		ctx->refstr[HSPCTX_REFSTR_MAX - 1] = 0;
		ctx->refdval = 2.5;
		code_callback( label2 );
		break;
		}
	default:
		throw HSPERR_SYNTAX;
	}

	return RUNMODE_RUN;
}

void hsp3typeinit_coretest_extcmd( HSP3TYPEINFO *info )
{
	ctx = info->hspctx;
	info->cmdfunc = cmdfunc_coretest;
}
