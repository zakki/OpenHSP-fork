/*--------------------------------------------------------
	HSP3dish main (raspberry pi/raspbian/OpenGLES)
  --------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <fcntl.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>
//#include <termios.h>

#include <errno.h>
//#include <regex.h>
//#include <dirent.h>
//#include <linux/input.h>
#include <stdbool.h>

#include <pico/time.h>


#if defined( __GNUC__ )
#include <ctype.h>
#endif

//#include "bcm_host.h"

#include "hsp3dish.h"
#include "../../hsp3/hsp3config.h"
#include "../../hsp3/strbuf.h"
#include "../../hsp3/hsp3.h"
#include "hsp3gr_rpipico.h"
#include "../supio.h"
#include "../hgio.h"
#include "../sysreq.h"
//#include "../hsp3ext.h"
#include "../../hsp3/strnote.h"
#include "../../hsp3/linux/hsp3ext_sock.h"

//#include "../emscripten/appengine.h"

#include "../../hsp3/linux/devctrl_io.h"
#define DEVCTRL_IO			// GPIO,I2C control

//typedef BOOL (CALLBACK *HSP3DBGFUNC)(HSP3DEBUG *,int,int,int);

extern char *hsp_mainpath;

/*----------------------------------------------------------*/

static Hsp3 *hsp = NULL;
static HSPCTX *ctx;
static HSPEXINFO *exinfo;								// Info for Plugins

static char fpas[]={ 'H'-48,'S'-48,'P'-48,'H'-48,
					 'E'-48,'D'-48,'~'-48,'~'-48 };
static char optmes[] = "HSPHED~~\0_1_________2_________3______";

static int hsp_wx, hsp_wy, hsp_wd, hsp_ss;
static int drawflag;
static int hsp_fps;
static int hsp_limit_step_per_frame;
static std::string syncdir;
static bool fs_initialized = false;

static int cl_option;
static char *cl_cmdline = "";
static char *cl_modname = "";

#ifndef HSPDEBUG
static int hsp_sscnt, hsp_ssx, hsp_ssy;
#endif

/*----------------------------------------------------------*/

#define MAX_INIFILE_LINESTR 1024

static	char *mem_inifile = NULL;
static	CStrNote *note_ini = NULL;
static	int lines_inifile;
static	char s_inifile[MAX_INIFILE_LINESTR];

static void	CloseIniFile( void )
{
	if ( mem_inifile != NULL ) {
		mem_bye( mem_inifile );
		mem_inifile = NULL;
	}
	if ( note_ini != NULL ) {
		delete note_ini;
		note_ini = NULL;
	}
}

static int	OpenIniFile( char *fname )
{
	CloseIniFile();
	mem_inifile = dpm_readalloc( fname );
	if ( mem_inifile == NULL ) return -1;
	note_ini = new CStrNote;
	note_ini->Select( mem_inifile );
	lines_inifile = note_ini->GetMaxLine();
	return 0;
}

static char *GetIniFileStr( char *keyword )
{
	int i;
	char *s;
	for(i=0;i<lines_inifile;i++) {
		note_ini->GetLine( s_inifile, i, MAX_INIFILE_LINESTR );
		if ( strncmp( s_inifile, keyword, strlen(keyword) ) == 0 ) {
			s = strchr2( s_inifile, '=' ) + 1;
			return s;
		}
	}
	return NULL;
}

static int	GetIniFileInt( char *keyword )
{
	char *s;
	s = GetIniFileStr( keyword );
	if ( s == NULL ) return 0;
	return atoi( s );
}

/*----------------------------------------------------------*/
static int quit_flag = 0;

void hsp3dish_dialog( char *mes )
{
	//MessageBox( NULL, mes, "Error",MB_ICONEXCLAMATION | MB_OK );
	printf( "%s\r\n", mes );
}


#ifdef HSPDEBUG
char *hsp3dish_debug( int type )
{
	//		デバッグ情報取得
	//
	char *p;
	p = code_inidbg();

	switch( type ) {
	case DEBUGINFO_GENERAL:
//		hsp3gr_dbg_gui();
		code_dbg_global();
		break;
	case DEBUGINFO_VARNAME:
		break;
	case DEBUGINFO_INTINFO:
		break;
	case DEBUGINFO_GRINFO:
		break;
	case DEBUGINFO_MMINFO:
		break;
	}
	return p;
}
#endif


void hsp3dish_drawon( void )
{
	//		描画開始指示
	//
	if ( drawflag == 0 ) {
		hgio_render_start();
		drawflag = 1;
	}
}


void hsp3dish_drawoff( void )
{
	//		描画終了指示
	//
	if ( drawflag ) {
		hgio_render_end();
		drawflag = 0;
	}
}


int hsp3dish_debugopen( void )
{
	return 0;
}

int hsp3dish_wait( int tick )
{
	//		時間待ち(wait)
	//		(awaitに変換します)
	//
	if ( ctx->waitcount <= 0 ) {
		ctx->runmode = RUNMODE_RUN;
		return RUNMODE_RUN;
	}
	ctx->waittick = tick + ( ctx->waitcount * 10 );
	return RUNMODE_AWAIT;
}


int hsp3dish_await( int tick )
{
	//		時間待ち(await)
	//
	if ( ctx->waittick < 0 ) {
		if ( ctx->lasttick == 0 ) ctx->lasttick = tick;
		ctx->waittick = ctx->lasttick + ctx->waitcount;
	}
	if ( tick >= ctx->waittick ) {
		ctx->lasttick = tick;
		ctx->runmode = RUNMODE_RUN;
		return RUNMODE_RUN;
	}
	return RUNMODE_AWAIT;
}


void hsp3dish_msgfunc( HSPCTX *hspctx )
{
	int tick;
	useconds_t usec;

#if 0
	updateKeyboard();
	hgio_touch( mouse_x, mouse_y, mouse_btn1 );
#endif

	//int x, y, btn;
	//btn = get_mouse(&x, &y);
	//hgio_touch( x, y, btn );
#ifdef HSPDEBUG
	//if ( btn ) {
	//	hspctx->runmode = RUNMODE_END;
	//}
	//if ( get_key_state(KEY_ESCAPE) ){	;	// [esc] to Quit
	if ( quit_flag ){	;	// Quit
		hspctx->runmode = RUNMODE_END;
		return;
	}
#endif

	while(1) {
		// logmes なら先に処理する
		if ( hspctx->runmode == RUNMODE_LOGMES ) {
			hspctx->runmode = RUNMODE_RUN;
			return;
		}

		switch( hspctx->runmode ) {
		case RUNMODE_STOP:
			return;
		case RUNMODE_WAIT:
			tick = hgio_gettick();
			hspctx->runmode = code_exec_wait( tick );
		case RUNMODE_AWAIT:
			//	高精度タイマー
			tick = hgio_gettick();					// すこし早めに抜けるようにする
			if ( code_exec_await( tick ) != RUNMODE_RUN ) {
				usec = ( hspctx->waittick - tick) / 2;
				sleep_us( usec*1000 );
			} else {
				tick = hgio_gettick();
				while( tick < hspctx->waittick ) {	// 細かいwaitを取る
					sleep_us( 1000 );
					tick = hgio_gettick();
				}
				hspctx->lasttick = tick;
				hspctx->runmode = RUNMODE_RUN;
#ifndef HSPDEBUG
				if ( ctx->hspstat & HSPSTAT_SSAVER ) {
					if ( hsp_sscnt ) hsp_sscnt--;
				}
#endif
			}
			break;
//		case RUNMODE_END:
//			throw HSPERR_NONE;
		case RUNMODE_RETURN:
			throw HSPERR_RETURN_WITHOUT_GOSUB;
		case RUNMODE_INTJUMP:
			throw HSPERR_INTJUMP;
		case RUNMODE_ASSERT:
			hspctx->runmode = RUNMODE_STOP;
#ifdef HSPDEBUG
			hsp3dish_debugopen();
#endif
			break;
	//	case RUNMODE_LOGMES:
		case RUNMODE_RESTART:
		{
			//	rebuild window

			hspctx->runmode = RUNMODE_RUN;
			break;
		}
		default:
			return;
		}
	}
}

/*----------------------------------------------------------*/

int hsp3dish_init( char *startfile )
{
	//		システム関連の初期化
	//		( mode:0=debug/1=release )
	//
	int a,orgexe, mode;
	int hsp_sum, hsp_dec;
	int autoscale,sx,sy;
	char a1;
#ifdef HSPDEBUG
	int i;
#endif
	InitSysReq();

#ifdef HSPDISHGP
	SetSysReq( SYSREQ_MAXMATERIAL, 64 );            // マテリアルのデフォルト値

	game = NULL;
	platform = NULL;
#endif

	//		HSP関連の初期化
	//
	hsp = new Hsp3();

	if ( startfile != NULL ) {
		hsp->SetFileName( startfile );
	}

	//		実行ファイルかデバッグ中かを調べる
	//
	mode = 0;
	orgexe=0;
	hsp_wx = 640;
	hsp_wy = 480;
	hsp_wd = 0;
	hsp_ss = 0;

	for( a=0 ; a<8; a++) {
		a1=optmes[a]-48;if (a1==fpas[a]) orgexe++;
	}
	if ( orgexe == 0 ) {
		mode = atoi(optmes+9) + 0x10000;
		a1=*(optmes+17);
		if ( a1 == 's' ) hsp_ss = HSPSTAT_SSAVER;
		hsp_wx=*(short *)(optmes+20);
		hsp_wy=*(short *)(optmes+23);
		hsp_wd=( *(short *)(optmes+26) );
		hsp_sum=*(unsigned short *)(optmes+29);
		hsp_dec=*(int *)(optmes+32);
		hsp->SetPackValue( hsp_sum, hsp_dec );
	}

	if ( hsp->Reset( mode ) ) {
		hsp3dish_dialog( "Startup failed." );
		return 1;
	}

	ctx = &hsp->hspctx;

	//		コマンドライン関連
	hsp->SetCommandLinePrm( cl_cmdline );		// コマンドラインパラメーターを保存
	hsp->SetModuleFilePrm( cl_modname );			// モジュール名を保存

	hsp3typeinit_dllcmd( code_gettypeinfo( TYPE_DLLFUNC ) );
	hsp3typeinit_dllctrl( code_gettypeinfo( TYPE_DLLCTRL ) );

#if 0
	//		Initalize Window
	//
	hsp3dish_initwindow( &mem_engine, -1, -1, "HSPDish ver" hspver );
	sx = (int)mem_engine.width;
	sy = (int)mem_engine.height;
	autoscale = 0;

	initKeyboard();
#endif

	//		Register Type
	//
	drawflag = 0;
	ctx->msgfunc = hsp3dish_msgfunc;

#if 0
//#ifdef HSPDEBUG
	if ( OpenIniFile( "hsp3dish.ini" ) == 0 ) {
		int iprm;
		iprm = GetIniFileInt( "wx" );if ( iprm > 0 ) hsp_wx = iprm;
		iprm = GetIniFileInt( "wy" );if ( iprm > 0 ) hsp_wy = iprm;
		iprm = GetIniFileInt( "vx" );if ( iprm > 0 ) sx = iprm;
		iprm = GetIniFileInt( "vy" );if ( iprm > 0 ) sy = iprm;
		iprm = GetIniFileInt( "autoscale" );if ( iprm > 0 ) autoscale = iprm;
		CloseIniFile();
	}

	if ( sx == 0 ) sx = hsp_wx;
	if ( sy == 0 ) sy = hsp_wy;

//#endif

	if ( sx != hsp_wx || sy != hsp_wy ) {
#ifndef HSPDISHGP
		hgio_view( hsp_wx, hsp_wy );
		hgio_size( sx, sy );
		hgio_autoscale( autoscale );
#endif
	}
#endif

	hsp3typeinit_dllcmd( code_gettypeinfo( TYPE_DLLFUNC ) );
	hsp3typeinit_dllctrl( code_gettypeinfo( TYPE_DLLCTRL ) );

	//		Initalize GUI System
	//
	// hsp3typeinit_extcmd( code_gettypeinfo( TYPE_EXTCMD ) );
	// hsp3typeinit_extfunc( code_gettypeinfo( TYPE_EXTSYSVAR ) );
	hsp3typeinit_cl_extcmd( code_gettypeinfo( TYPE_EXTCMD ) );
	hsp3typeinit_cl_extfunc( code_gettypeinfo( TYPE_EXTSYSVAR ) );


	exinfo = ctx->exinfo2;

#ifdef USE_OBAQ
	HSP3TYPEINFO *tinfo = code_gettypeinfo( TYPE_USERDEF );
	tinfo->hspctx = ctx;
	tinfo->hspexinfo = exinfo;
	hsp3typeinit_dw_extcmd( tinfo );
#endif

#if 0
	{
	HSP3TYPEINFO *tinfo = code_gettypeinfo( TYPE_USERDEF+1 );
	tinfo->hspctx = ctx;
	tinfo->hspexinfo = exinfo;
	hsp3typeinit_sock_extcmd( tinfo );
	}
#endif

	//		Initalize DEVINFO
#ifdef DEVCTRL_IO
	HSP3DEVINFO *devinfo;
	devinfo = hsp3extcmd_getdevinfo();
	hsp3dish_setdevinfo_io( devinfo );
#endif
	return 0;
}


static void hsp3dish_bye( void )
{
#ifdef DEVCTRL_IO
	hsp3dish_termdevinfo_io();
#endif

#if 0
	bcm_host_deinit();
#endif

	//		HSP関連の解放
	//
	if ( hsp != NULL ) { delete hsp; hsp = NULL; }
}


void hsp3dish_error( void )
{
	char errmsg[1024];
	char *msg;
	char *fname;
	HSPERROR err;
	int ln;
	err = code_geterror();
	ln = code_getdebug_line();
	msg = hspd_geterror(err);
	fname = code_getdebug_name();

	if ( ln < 0 ) {
		sprintf( errmsg, "#Error %d\n-->%s\n",(int)err,msg );
		fname = NULL;
	} else {
		sprintf( errmsg, "#Error %d in line %d (%s)\n-->%s\n",(int)err, ln, fname, msg );
	}
	hsp3dish_debugopen();
	hsp3dish_dialog( errmsg );
}


char *hsp3dish_getlog(void)
{
#ifdef HSPDISHGP
	return (char *)gplog.c_str();
#else
	return "";
#endif
}


int hsp3dish_exec( void )
{
	//		実行メインを呼び出す
	//
	int runmode;
	int endcode;

	hsp3dish_msgfunc( ctx );

	//		実行の開始
	//
	runmode = code_execcmd();
	if ( runmode == RUNMODE_ERROR ) {
		try {
			hsp3dish_error();
		}
		catch( ... ) {
		}
		hsp3dish_bye();
		return -1;
	}

	endcode = ctx->endcode;
	hsp3dish_bye();
	return endcode;
}


void hsp3dish_option( int opt )
{
	//		HSP3オプション設定
	//
	cl_option = opt;
}


void hsp3dish_cmdline( char *cmdline )
{
	//		HSP3オプション設定
	//
	cl_cmdline = cmdline;						// コマンドラインパラメーターを入れる
}


void hsp3dish_modname( char *modname )
{
	//		HSP3オプション設定
	//
	cl_modname = modname;						// arg[0]パラメーターを入れる
}
