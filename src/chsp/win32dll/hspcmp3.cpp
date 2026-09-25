//
//		HSP compile/package functions for HSP3
//				onion software/onitama 2002-2017
//

#include <stdio.h>
#include <windows.h>
#include <direct.h>
#include <string.h>

#include "../../hsp3/hsp3config.h"
#include "../../hsp3/hsp3debug.h"			// hsp3 error code
#include "../../hsp3/hsp3struct.h"			// hsp3 core define
#include "../../hsp3/hspwnd.h"				// hsp3 windows define

#include <string>
#include <vector>
#include "../../hspcmp/chsp/chsp_pipeline.h"
#include "../../hspcmp/supio.h"

//	VC++の場合
#ifdef __cplusplus
#define EXPORT extern "C" __declspec (dllexport)
#else
#define EXPORT __declspec (dllexport)
#endif

static char fname[_MAX_PATH];
static char rname[_MAX_PATH];
static char oname[_MAX_PATH];
static int orgcompath = 0;
static char compath[_MAX_PATH];

enum
{
	HSC3_CHSP_COMPILE_NONE = 64,
};

static std::string g_last_error_message;
static bool g_use_delegate_error_message = false;
static char analysis_keyword[_MAX_PATH];
static char* analysis_name = NULL;
static int analysis_mode = 0;
static int analysis_line = 0;

extern "C" IMAGE_DOS_HEADER __ImageBase;

typedef BOOL (WINAPI *HscIniFunc)( BMSCR *, char *, int, int );
typedef BOOL (WINAPI *HscAnalysisFunc)( BMSCR *, char *, int, int );
typedef BOOL (WINAPI *HscGetMesFunc)( char *, int, int, int );
typedef BOOL (WINAPI *HscClrMesFunc)( int, int, int, int );
typedef BOOL (WINAPI *HscCompFunc)( int, int, int, int );
typedef BOOL (WINAPI *HscMessizeFunc)( int *, int, int, int );
typedef BOOL (WINAPI *FourIntFunc)( int, int, int, int );
typedef BOOL (WINAPI *CharInt3Func)( char *, int, int, int );
typedef BOOL (WINAPI *IntIntIntCharFunc)( int, int, int, char * );
typedef BOOL (WINAPI *IntPtrInt3Func)( int *, int, int, int );
typedef BOOL (WINAPI *CharCharInt2Func)( char *, char *, int, int );
typedef BOOL (WINAPI *IntPtrCharInt2Func)( int *, char *, int, int );
typedef BOOL (WINAPI *HspexInt3Func)( HSPEXINFO *, int, int, int );

struct HspcmpDelegateApi
{
	HMODULE module;
	int attempted;
	HscIniFunc hsc_ini;
	HscIniFunc hsc_refname;
	HscIniFunc hsc_objname;
	HscAnalysisFunc hsc3_analysis;
	HscIniFunc hsc_compath;
	HscGetMesFunc hsc_getmes;
	HscClrMesFunc hsc_clrmes;
	HscCompFunc hsc_comp;
	HscMessizeFunc hsc3_messize;
};

static HspcmpDelegateApi delegate_api = {};

#if defined( __GNUC__ ) && defined( __cplusplus )
extern "C"
#endif
BOOL WINAPI DllMain (HINSTANCE hInstance, DWORD fdwReason, PVOID pvReserved)
{
	if ( fdwReason == DLL_PROCESS_ATTACH ) {
	}
	if ( fdwReason == DLL_PROCESS_DETACH ) {
		if ( delegate_api.module != NULL ) {
			FreeLibrary( delegate_api.module );
			delegate_api = {};
		}
	}
	return TRUE;
}

//----------------------------------------------------------

static int GetFilePath( char *bname )
{
	//		フルパス名から、ファイルパスの取得(\を残す)
	int a, b, len;
	char a1;
	b = -1;
	len = (int)strlen( bname );
	for ( a = 0; a < len; a++ ) {
		a1 = bname[a];
		if ( a1 == '\\' ) b = a;
		if ( a1 < 0 ) a++;
	}
	if ( b < 0 ) return 1;
	bname[b + 1] = 0;
	return 0;
}

void cutext( char *st )
{
	int r = -1, f = 0;
	for ( int i = 0; st[i] != '\0'; ++i ) {
		if ( f ) {
			f = 0;
		} else {
			if ( st[i] == '.' ) {
				r = i;
			} else if ( st[i] == '\\' || st[i] == '/' ) {
				r = -1;
			}
			f = ( ( (unsigned char)st[i] >= 0x81 && (unsigned char)st[i] <= 0x9f ) ||
			      ( (unsigned char)st[i] >= 0xe0 && (unsigned char)st[i] <= 0xfc ) );
		}
	}
	if ( r >= 0 ) st[r] = '\0';
}

static int load_delegate_api( void )
{
	if ( delegate_api.attempted ) {
		return delegate_api.module != NULL;
	}
	delegate_api.attempted = 1;

	char dll_path[_MAX_PATH] = { 0 };
	const char *env_orig = getenv( "HSPCMP_ORIGINAL" );
	if ( env_orig != NULL && env_orig[0] != '\0' ) {
		strncpy( dll_path, env_orig, _MAX_PATH - 1 );
		delegate_api.module = LoadLibraryA( dll_path );
	}
	if ( delegate_api.module == NULL ) {
		if ( GetModuleFileNameA( (HMODULE)&__ImageBase, dll_path, _MAX_PATH ) != 0 ) {
			if ( GetFilePath( dll_path ) == 0 ) {
				strcat( dll_path, "hspcmp_original.dll" );
				delegate_api.module = LoadLibraryA( dll_path );
			}
		}
	}
	if ( delegate_api.module == NULL ) {
		return 0;
	}

	delegate_api.hsc_ini = (HscIniFunc)GetProcAddress( delegate_api.module, "_hsc_ini@16" );
	delegate_api.hsc_refname = (HscIniFunc)GetProcAddress( delegate_api.module, "_hsc_refname@16" );
	delegate_api.hsc_objname = (HscIniFunc)GetProcAddress( delegate_api.module, "_hsc_objname@16" );
	delegate_api.hsc3_analysis = (HscAnalysisFunc)GetProcAddress( delegate_api.module, "_hsc3_analysis@16" );
	delegate_api.hsc_compath = (HscIniFunc)GetProcAddress( delegate_api.module, "_hsc_compath@16" );
	delegate_api.hsc_getmes = (HscGetMesFunc)GetProcAddress( delegate_api.module, "_hsc_getmes@16" );
	delegate_api.hsc_clrmes = (HscClrMesFunc)GetProcAddress( delegate_api.module, "_hsc_clrmes@16" );
	delegate_api.hsc_comp = (HscCompFunc)GetProcAddress( delegate_api.module, "_hsc_comp@16" );
	delegate_api.hsc3_messize = (HscMessizeFunc)GetProcAddress( delegate_api.module, "_hsc3_messize@16" );
	if ( delegate_api.hsc_ini == NULL || delegate_api.hsc_refname == NULL || delegate_api.hsc_objname == NULL ||
		 delegate_api.hsc3_analysis == NULL || delegate_api.hsc_compath == NULL || delegate_api.hsc_getmes == NULL ||
		 delegate_api.hsc_clrmes == NULL || delegate_api.hsc_comp == NULL || delegate_api.hsc3_messize == NULL ) {
		FreeLibrary( delegate_api.module );
		delegate_api = {};
		delegate_api.attempted = 1;
		return 0;
	}

	return 1;
}

static void forward_state_to_delegate( BMSCR *bm )
{
	if ( load_delegate_api() == 0 ) return;
	delegate_api.hsc_refname( bm, rname, 0, 0 );
	delegate_api.hsc_objname( bm, oname, 0, 0 );
	if ( orgcompath ) {
		delegate_api.hsc_compath( bm, compath, 0, 0 );
	}
	if ( analysis_name != NULL || analysis_mode != 0 || analysis_line != 0 ) {
		char *name = analysis_name != NULL ? analysis_name : (char *)"";
		delegate_api.hsc3_analysis( bm, name, analysis_mode, analysis_line );
	}
}

template <typename T> static T get_delegate_proc( const char *name )
{
	if ( load_delegate_api() == 0 ) return NULL;
	return reinterpret_cast<T>( GetProcAddress( delegate_api.module, name ) );
}

//----------------------------------------------------------

EXPORT BOOL WINAPI hsc_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc_ini "src-file"  (type6)
	//
	strcpy( fname, p1 );
	strcpy( rname, p1 );
	strcpy( oname, p1 );
	cutext( oname );
	strcat( oname, ".ax" );
	analysis_name = NULL;
	analysis_mode = 0;
	analysis_line = 0;
	if ( load_delegate_api() ) {
		delegate_api.hsc_ini( bm, p1, p2, p3 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_refname ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc_refname "ref-file"  (type6)
	//
	strcpy( rname, p1 );
	if ( load_delegate_api() ) {
		delegate_api.hsc_refname( bm, p1, p2, p3 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_objname( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc_objname "obj-file"  (type6)
	//
	strcpy( oname, p1 );
	if ( load_delegate_api() ) {
		delegate_api.hsc_objname( bm, p1, p2, p3 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc3_analysis( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc3_analysisname "name", mode, line  (type6)
	//
	if ( p1 == NULL || *p1 == 0 ) {
		analysis_name = NULL;
	} else {
		strncpy( analysis_keyword, p1, _MAX_PATH - 1 );
		analysis_keyword[_MAX_PATH - 1] = 0;
		analysis_name = analysis_keyword;
	}
	analysis_mode = p2;
	analysis_line = p3;
	if ( load_delegate_api() ) {
		delegate_api.hsc3_analysis( bm, p1, p2, p3 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc3_kwlineinfo( char *p1, int p2, int p3, int p4 )
{
	//
	//		hsc3_kwlineinfo val, opt (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_kwlineinfo@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( p1 != NULL ) *p1 = 0;
	return -1;
}

EXPORT BOOL WINAPI hsc_ver ( int p1, int p2, int p3, char *p4 )
{
	//
	//		hsc_ver (type$10)
	//
	IntIntIntCharFunc delegate = get_delegate_proc<IntIntIntCharFunc>( "_hsc_ver@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( p4 != NULL ) {
		sprintf( p4, "cHSP code generator ver%s", hspver );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_bye ( int p1, int p2, int p3, int p4 )
{
	//
	//		hsc_bye (type$100)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_hsc_bye@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_getmes ( char *p1, int p2, int p3, int p4 )
{
	//
	//		hsc_getmes val (type1)
	//
	if ( g_use_delegate_error_message && load_delegate_api() ) {
		return delegate_api.hsc_getmes( p1, p2, p3, p4 );
	}
	if ( p1 != NULL ) {
		strcpy( p1, g_last_error_message.c_str() );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_clrmes ( int p1, int p2, int p3, int p4 )
{
	//
	//		hsc_clrmes (type0)
	//
	g_last_error_message.clear();
	g_use_delegate_error_message = false;
	if ( load_delegate_api() ) {
		delegate_api.hsc_clrmes( p1, p2, p3, p4 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_compath ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc_compath "common-path"  (type6)
	//
	strcpy( compath, p1 );
	orgcompath = 1;
	if ( load_delegate_api() ) {
		delegate_api.hsc_compath( bm, p1, p2, p3 );
	}
	return 0;
}

EXPORT BOOL WINAPI hsc_comp ( int p1, int p2, int p3, int p4 )
{
	g_last_error_message.clear();
	g_use_delegate_error_message = false;

	if ( !load_delegate_api() ) {
		g_last_error_message = "#Cannot load original hspcmp DLL (hspcmp_original.dll).";
		return -1;
	}

	if ( orgcompath == 0 ) {
		GetModuleFileNameA( NULL, compath, _MAX_PATH );
		GetFilePath( compath );
		strcat( compath, "common\\" );
	}

	if ( ChspPipeline::ContainsChspInFile( fname ) ) {
		ChspPipelineOptions opts;
		opts.source_path = fname;
		opts.output_ax_path = oname;
		opts.common_path = compath;
		opts.compile_mode = ( p2 & HSC3_CHSP_COMPILE_NONE )
							? ChspPipelineCompileMode::None
							: ChspPipelineCompileMode::Libtcc;
		opts.debug_mode = ( p1 & 1 ) != 0;
		opts.keep_intermediate = false;

		ChspPipelineResult pipe_res = ChspPipeline::Process( opts );
		if ( !pipe_res.success ) {
			g_last_error_message = pipe_res.error_message;
			return -1;
		}

		if ( pipe_res.has_chsp ) {
			delegate_api.hsc_clrmes( 0, 0, 0, 0 );
			delegate_api.hsc_ini( NULL, const_cast<char *>( pipe_res.intermediate_hsp_path.c_str() ), 0, 0 );
			forward_state_to_delegate( NULL );
			int st = delegate_api.hsc_comp( p1, p2, p3, p4 );
			g_use_delegate_error_message = true;

			ChspPipeline::CleanupIntermediate( pipe_res );
			return st;
		}
	}

	delegate_api.hsc_clrmes( 0, 0, 0, 0 );
	delegate_api.hsc_ini( NULL, fname, 0, 0 );
	forward_state_to_delegate( NULL );
	int st = delegate_api.hsc_comp( p1, p2, p3, p4 );
	g_use_delegate_error_message = true;
	return st;
}

EXPORT BOOL WINAPI pack_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_ini@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_view ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_view@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_make ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_make@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_opt ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_opt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_rt ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_rt@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_exe ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_exe@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI pack_get ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_get@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_getsym( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_hsc3_getsym@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_kwlbuf( char *p1, int p2, int p3, int p4 )
{
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_kwlbuf@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_kwlsize( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_hsc3_kwlsize@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_kwlclose( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_hsc3_kwlclose@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_messize ( int *p1, int p2, int p3, int p4 )
{
	if ( g_use_delegate_error_message && load_delegate_api() ) {
		return delegate_api.hsc3_messize( p1, p2, p3, p4 );
	}
	if ( p1 != NULL ) {
		*p1 = (int)g_last_error_message.size() + 1;
	}
	return 0;
}

EXPORT BOOL WINAPI hsc3_make ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_hsc3_make@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI hsc3_getruntime ( char *p1, char *p2, int p3, int p4 )
{
	CharCharInt2Func delegate = get_delegate_proc<CharCharInt2Func>( "_hsc3_getruntime@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( p1 != NULL ) *p1 = 0;
	return -1;
}

EXPORT BOOL WINAPI hsc3_run ( char *p1, int p2, int p3, int p4 )
{
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_run@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int i = WinExec( p1, SW_SHOW );
	if ( i < 32 ) return -1;
	return 0;
}

EXPORT BOOL WINAPI aht_source( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_source@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_ini@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_stdbuf ( char *p1, int p2, int p3, int p4 )
{
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_stdbuf@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_stdsize ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_stdsize@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getopt( char *p1, char *p2, int p3, int p4 )
{
	CharCharInt2Func delegate = get_delegate_proc<CharCharInt2Func>( "_aht_getopt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getpropcnt ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getpropcnt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getpropid ( int *p1, char *p2, int p3, int p4 )
{
	IntPtrCharInt2Func delegate = get_delegate_proc<IntPtrCharInt2Func>( "_aht_getpropid@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getprop( char *p1, int p2, int p3, int p4 )
{
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_getprop@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getproptype ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getproptype@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getpropmode ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getpropmode@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_make ( int *p1, char *p2, int p3, int p4 )
{
	IntPtrCharInt2Func delegate = get_delegate_proc<IntPtrCharInt2Func>( "_aht_make@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_makeinit ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_makeinit@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_makeend ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_makeend@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_makeput ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_makeput@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_setprop ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_setprop@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_sendstr ( char *p1, int p2, int p3, int p4 )
{
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_sendstr@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getmodcnt ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getmodcnt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getmodaxis ( int *p1, int p2, int p3, int p4 )
{
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getmodaxis@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_setmodaxis ( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_setmodaxis@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_prjload ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_prjload@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_prjsave ( BMSCR *bm, char *p1, int p2, int p3 )
{
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_prjsave@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getprjmax( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getprjmax@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getprjsrc( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getprjsrc@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_prjload2( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_prjload2@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_prjloade( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_prjloade@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_delmod( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_delmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_linkmod( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_linkmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_unlinkmod( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_unlinkmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_setpage( int p1, int p2, int p3, int p4 )
{
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_setpage@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getpage( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getpage@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_propupdate( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_propupdate@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_parts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_parts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_listparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_listparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_findstart( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findstart@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_findparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_findend( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findend@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI aht_getexid( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getexid@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI hman_init( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_init@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI hman_search( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_search@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}

EXPORT BOOL WINAPI hman_getresult( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_getresult@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	return -1;
}
