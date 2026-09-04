
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
#include "../../hspcmp/chsp/chsp_pipeline.h"
#include "../../hspcmp/supio.h"

#define USE_HSMANAGER
#ifdef USE_HSMANAGER
#include "../hsmanager.h"
static HspHelpManager hsman;
#endif

//#define DPM_SUPPORT		// DPMファイルマネージャをサポート
#ifdef DPM_SUPPORT
#include "dpm.h"
#endif

#define DPM2_SUPPORT		// DPM2ファイルマネージャをサポート
#include "../../hsp3/filepack.h"
#define PACKFILE "packfile"
#define DPMFILE "data"

#define ICONINS_SUPPORT	// ICONINSツールをサポート

//	VC++の場合
#ifdef __cplusplus
#define EXPORT extern "C" __declspec (dllexport)
#else
#define EXPORT __declspec (dllexport)
#endif

static char fname[_MAX_PATH];
static char rname[_MAX_PATH];
static char oname[_MAX_PATH];
static char hspexe[_MAX_PATH];
static int opt1,opt2,opt3;

static int orgcompath=0;
static char compath[_MAX_PATH];
static char delegated_input[_MAX_PATH];

enum class ChspNativeCompileMode
{
	None,
	Libtcc,
};

enum
{
	HSC3_CHSP_COMPILE_NONE = 64,
};

static std::string g_last_error_message;
static bool g_use_delegate_error_message = false;
static int homeid;				// Home object
static int ahtbuild_error;		// Error code
static char analysis_keyword[_MAX_PATH];
static char* analysis_name;
static int analysis_mode;
static int analysis_line;
static int last_delegate_error_message;

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

extern char *hsp_prestr[];

#ifdef DPM2_SUPPORT
static FilePack filepack;		// File Pack Manager
#endif


#if defined( __GNUC__ ) && defined( __cplusplus )
extern "C"
#endif
BOOL WINAPI DllMain (HINSTANCE hInstance, DWORD fdwReason, PVOID pvReserved)
{
	if ( fdwReason==DLL_PROCESS_ATTACH ) {
	}
	if ( fdwReason==DLL_PROCESS_DETACH ) {
		if ( delegate_api.module != NULL ) {
			FreeLibrary( delegate_api.module );
			delegate_api = {};
		}
	}
	return TRUE ;
}

//----------------------------------------------------------

static int GetFilePath( char *bname )
{
	//		フルパス名から、ファイルパスの取得(\を残す)
	//
	int a,b,len;
	char a1;
	b=-1;
	len=(int)strlen(bname);
	for(a=0;a<len;a++) {
		a1=bname[a];
		if (a1=='\\') b=a;
		if (a1<0) a++; 
	}
	if (b<0) return 1;
	bname[b+1]=0;
	return 0;
}


static int load_delegate_api( void )
{
	char dll_path[_MAX_PATH];

	if ( delegate_api.attempted ) {
		return delegate_api.module != NULL;
	}
	delegate_api.attempted = 1;

	if ( GetModuleFileNameA( (HMODULE)&__ImageBase, dll_path, _MAX_PATH ) == 0 ) {
		return 0;
	}
	if ( GetFilePath( dll_path ) != 0 ) {
		return 0;
	}
	strcat( dll_path, "hspcmp_original.dll" );
	delegate_api.module = LoadLibraryA( dll_path );
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


static void set_delegate_message_mode( int enabled )
{
	last_delegate_error_message = enabled;
}


template <typename T> static T get_delegate_proc( const char *name )
{
	if ( load_delegate_api() == 0 ) return NULL;
	return reinterpret_cast<T>( GetProcAddress( delegate_api.module, name ) );
}

/*
	rev 54
	supio_win.cpp と supio_linux.cpp でも cutext が定義されている。
	そちらを使うようにしてここの cutext は削除。
*/


static void _sendstr( HWND hw, char *p1 )
{
	//		Win9x用
	LPARAM lprm;
	char *mes;
	char a1;
	int a;

	mes = p1;
	lprm=1;
	a=0;
	while(1) {
		a1=mes[a];if (a1==0) break;
		PostMessage( hw,WM_CHAR, a1, lprm );
		a++;
	}
}


static void _sendstr2( HWND hw, char *p1 )
{
	//		Win2000以降用
	LPARAM lprm;
	unsigned char *mes;
	unsigned char a1;
	int acode;
	int a;

	mes = (unsigned char *)p1;

	lprm=1;
	a=0;
	while(1) {
		a1=mes[a];if (a1==0) break;
		if ( a1 & 0x80 ) {
			acode = (int)mes[a+1];
			acode = (acode<<8) | a1;
			PostMessage( hw,WM_CHAR, acode, 0 );
			a+=2;
		} else {
			PostMessage( hw,WM_CHAR, a1, lprm );
			a++;
		}
	}
}


//----------------------------------------------------------

EXPORT BOOL WINAPI hsc_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc_ini "src-file"  (type6)
	//
	strcpy(fname,p1);
	strcpy(rname,p1);
	strcpy(oname,p1);
	cutext(oname);
	strcat(oname,".ax");
	analysis_name = NULL;
	analysis_mode = 0;
	analysis_line = 0;
	last_delegate_error_message = 0;
	delegated_input[0] = 0;
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
	strcpy(rname,p1);
	if ( load_delegate_api() ) {
		delegate_api.hsc_refname( bm, p1, p2, p3 );
	}
	return 0;
}


EXPORT BOOL WINAPI hsc_objname(BMSCR* bm, char* p1, int p2, int p3)
{
	//
	//		hsc_objname "obj-file"  (type6)
	//
	strcpy(oname, p1);
	if ( load_delegate_api() ) {
		delegate_api.hsc_objname( bm, p1, p2, p3 );
	}
	return 0;
}


EXPORT BOOL WINAPI hsc3_analysis(BMSCR* bm, char* p1, int p2, int p3)
{
	//
	//		hsc3_analysisname "name", mode, line  (type6)
	//
	if (*p1 == 0) {
		analysis_name = NULL;
	}
	else {
		strncpy(analysis_keyword, p1, _MAX_PATH - 1);
		analysis_keyword[_MAX_PATH - 1] = 0;
		analysis_name = analysis_keyword;
	}
	analysis_mode = p2;
	analysis_line = p3;
	hsc3->InitAnalysisInfo(analysis_mode, analysis_name, p3);
	if ( load_delegate_api() ) {
		delegate_api.hsc3_analysis( bm, p1, p2, p3 );
	}
	return 0;
}


EXPORT BOOL WINAPI hsc3_kwlineinfo(char* p1, int p2, int p3, int p4)
{
	//
	//		hsc3_kwlineinfo val, opt (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_kwlineinfo@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	strcpy(p1, hsc3->GetAnalysisLineInfo(p2));
	return 0;
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
	sprintf( p4,"%s ver%s", HSC3TITLE, hspver );
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
	strcpy( p1, g_last_error_message.c_str() );
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
	strcpy(compath,p1);
	orgcompath=1;
	if ( load_delegate_api() ) {
		delegate_api.hsc_compath( bm, p1, p2, p3 );
	}
	return 0;
}


static int hsc_comp_sub(int p1, int p2, int p3, int p4)
{
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

	if ( ChspPipeline::ContainsChspDirective( fname ) ) {
		ChspPipeline::Options opts;
		opts.common_path = compath;
		opts.compile_mode = ( p2 & HSC3_CHSP_COMPILE_NONE )
							? ChspPipeline::NativeCompileMode::None
							: ChspPipeline::NativeCompileMode::Libtcc;

		std::string intermediate_path;
		std::string chsp_err;
		if ( !ChspPipeline::Process( fname, opts, &intermediate_path, &chsp_err ) ) {
			g_last_error_message = chsp_err;
			return -1;
		}

		delegate_api.hsc_clrmes( 0, 0, 0, 0 );
		delegate_api.hsc_ini( NULL, const_cast<char *>( intermediate_path.c_str() ), 0, 0 );
		forward_state_to_delegate( NULL );
		int st = delegate_api.hsc_comp( p1, p2, p3, p4 );
		g_use_delegate_error_message = true;

		ChspPipeline::CleanupIntermediate( intermediate_path );
		return st;
	} else {
		delegate_api.hsc_clrmes( 0, 0, 0, 0 );
		delegate_api.hsc_ini( NULL, fname, 0, 0 );
		forward_state_to_delegate( NULL );
		int st = delegate_api.hsc_comp( p1, p2, p3, p4 );
		g_use_delegate_error_message = true;
		return st;
	}
}


//----------------------------------------------------------

EXPORT BOOL WINAPI pack_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		pack_ini "src-file"  (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_ini@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	strcpy(fname, p1);
	cutext(fname);
	if (hsc3 == NULL) Alert("#No way.");
	hsc3->ResetError();
	opt1 = 640; opt2 = 480; opt3 = 0;
	strcpy(hspexe, "hsprt");

#ifdef DPM_SUPPORT
	dpmc_ini( hsc3->errbuf, fname );
#endif
#ifdef DPM2_SUPPORT
	filepack.Reset();
	filepack.SetErrorBuffer(hsc3->errbuf);
#endif
	return 0;
}


EXPORT BOOL WINAPI pack_view ( int p1, int p2, int p3, int p4 )
{
	//
	//		pack_view encode  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_view@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int st;
	st = 0;
#ifdef DPM_SUPPORT
	st = dpmc_view();
#endif
#ifdef DPM2_SUPPORT
	char dpmname[_MAX_PATH];
	strcpy(dpmname,fname);
	strcat(dpmname, ".dpm");
	char tmp[1024];

	if (p1 == 0) p1 = -1;
	int res = filepack.LoadPackFile(dpmname, p1);
	if (res<0) {
		sprintf(tmp,"#Error %d in loading [%s].",res, dpmname);
		filepack.Print(tmp);
		st = 1;
	}
	else {
		sprintf(tmp, "#[%s] Loaded.", dpmname);
		filepack.Print(tmp);
		filepack.PrintFiles();
	}
#endif
	return -st;
}


EXPORT BOOL WINAPI pack_make ( int p1, int p2, int p3, int p4 )
{
	//
	//		pack_make mode, key(type0)
	//		     mode : (1=ForDPM/0=ForExecutable)
	//		     key  : (0=Default/other=New Seed)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_make@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int st;
	st = 0;
#ifdef DPM_SUPPORT
	if ( p2 != 0 ) dpmc_dpmkey( p2 );
	st=dpmc_pack(p1);
#endif

#ifdef DPM2_SUPPORT
	if (p2 == 0) p2 = -1;
#ifdef HSPWIN
	p1 = (int)GetTickCount();	// Windowsの場合はtickをシード値とする
#else
	p1 = (int)time(0);			// Windows以外のランダムシード値
#endif
	if (filepack.SavePackFile(fname, PACKFILE, p1, p2) < 0) {
		st = 1;
	}
#endif
	return -st;
}


EXPORT BOOL WINAPI pack_opt ( int p1, int p2, int p3, int p4 )
{
	//
	//		pack_opt sx,sy,disp_sw (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_opt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	opt1=p1;if (opt1==0) opt1=640;
	opt2=p2;if (opt2==0) opt2=480;
	opt3=p3;							// disp SW (1=blank window)
	return 0;
}


EXPORT BOOL WINAPI pack_rt ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		pack_rt "runtime-file"  (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_rt@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	strcpy(hspexe,p1);
	return 0;
}


EXPORT BOOL WINAPI pack_exe ( int p1, int p2, int p3, int p4 )
{
	//
	//		pack_exe mode (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_pack_exe@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int st;
	st = 0;
#ifdef DPM_SUPPORT
	st=dpmc_mkexe(p1,hspexe,opt1,opt2,opt3);
#endif
	return -st;
}


EXPORT BOOL WINAPI pack_get ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		pack_get "get-file", enc  (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_pack_get@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	int st;
	st = 0;
#ifdef DPM_SUPPORT
	st=dpmc_get(p1);
#endif
#ifdef DPM2_SUPPORT
	if (filepack.ExtractFile(p1,NULL,p2) < 0) {
		st = 1;
	}
#endif
	return -st;
}


//----------------------------------------------------------
//		Additional service on 2.6
//----------------------------------------------------------

EXPORT BOOL WINAPI hsc3_getsym(int p1, int p2, int p3, int p4)
{
	//
	//		hsc3_getsym val  (type1)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_hsc3_getsym@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	hsc3->ResetError();
	if (orgcompath == 0) {
		GetModuleFileName(NULL, compath, _MAX_PATH);
		GetFilePath(compath);
		strcat(compath, "common\\");
	}
	hsc3->SetCommonPath(compath);
	if (hsc3->GetCmdList(p1 | 2)) return -1;
	return 0;
}


EXPORT BOOL WINAPI hsc3_kwlbuf(char* p1, int p2, int p3, int p4)
{
	//
	//		hsc3_kwlbuf bufvar, maxsize  (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_kwlbuf@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	char* p = hsc3->GetAnalysisInfo();
	if (p == NULL) {
		return -1;
	}
	if (p2) {
		if (hsc3->GetAnalysisInfoSize() > p2) {
			return -1;
		}
	}
	strcpy(p1, p );
	return 0;
}


EXPORT BOOL WINAPI hsc3_kwlsize(int* p1, int p2, int p3, int p4)
{
	//
	//		hsc3_kwlsize var  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_hsc3_kwlsize@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	*p1 = hsc3->GetAnalysisInfoSize();
	if (*p1 == 0) return -1;
	return 0;
}


EXPORT BOOL WINAPI hsc3_kwlclose(int p1, int p2, int p3, int p4)
{
	//
	//		hsc3_kwlclose var  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_hsc3_kwlclose@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	hsc3->DeleteAnalysisInfo();
	return 0;
}


EXPORT BOOL WINAPI hsc3_messize ( int *p1, int p2, int p3, int p4 )
{
	//
	//		hsc3_messize val  (type1)
	//
	if ( last_delegate_error_message && load_delegate_api() ) {
		return delegate_api.hsc3_messize( p1, p2, p3, p4 );
	}
	*p1 = hsc3->GetErrorSize();
	return 0;
}


EXPORT BOOL WINAPI hsc3_make ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		hsc3_make "myname",sw,0  (type6)
	//		(sw=1の場合はiconinsを呼び出す)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_hsc3_make@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	char libpath[_MAX_PATH];
	int i,type;
	int opt3a,opt3b;
	int st;
#ifdef ICONINS_SUPPORT
	char ici_opt[4096];
	char ici_current[_MAX_PATH];
	char ici_target[_MAX_PATH];
	char ici_icon[_MAX_PATH];
	char ici_version[_MAX_PATH];
	char ici_manifest[_MAX_PATH];
	char ici_lang[_MAX_PATH];
	char ici_upx[_MAX_PATH];
	int ici_use_icon = 0;
	int ici_use_version = 0;
	int ici_use_manifest = 0;
	int ici_use_lang = 0;
	int ici_use_upx = 0;
#endif

	if ( hsc3==NULL ) Alert( "#No way." );
	hsc3->ResetError();

	strcpy( libpath, p1 );
	GetFilePath( libpath );

#ifdef ICONINS_SUPPORT
	GetModuleFileName( NULL,ici_opt,_MAX_PATH );
	GetFilePath( ici_opt );
	strcat( ici_opt, "iconins.exe" );
	GetCurrentDirectory( _MAX_PATH, ici_current );
	strcat( ici_current, "\\" );
#endif

	i = hsc3->OpenPackfile();
	if (i) { Alert( "packfileが見つかりません" ); return -1; }
	hsc3->GetPackfileOption( hspexe, "runtime", "hsprt" );
	strcat( libpath, hspexe );
	strcpy( hspexe, libpath );
	hsc3->GetPackfileOption( fname, "name", "hsptmp" );
	cutext( fname );
	type = hsc3->GetPackfileOptionInt( "type", 0 );
	opt1 = hsc3->GetPackfileOptionInt( "xsize", 640 );
	opt2 = hsc3->GetPackfileOptionInt( "ysize", 480 );
	opt3a = hsc3->GetPackfileOptionInt( "hide", 0 );
	opt3b = hsc3->GetPackfileOptionInt( "orgpath", 0 );
	opt3 = 0;
	if ( opt3a ) opt3 |= 1;
	if ( opt3b ) opt3 |= 2;

#ifdef ICONINS_SUPPORT
	hsc3->GetPackfileOption( ici_icon, "icon", "" );
	if ( ici_icon[0] != 0 ) { ici_use_icon = 1; }
	hsc3->GetPackfileOption( ici_version, "version", "" );
	if ( ici_version[0] != 0 ) { ici_use_version = 1; }
	hsc3->GetPackfileOption( ici_manifest, "manifest", "" );
	if ( ici_manifest[0] != 0 ) { ici_use_manifest = 1; }
	hsc3->GetPackfileOption( ici_lang, "lang", "" );
	if ( ici_lang[0] != 0 ) { ici_use_lang = 1; }
	hsc3->GetPackfileOption( ici_upx, "upx", "" );
	if ( ici_upx[0] != 0 ) { ici_use_upx = 1; }

	strcpy( ici_target, ici_current );
	strcat( ici_target, fname );
	if (type==2) strcat(ici_target,".scr");
			else strcat(ici_target,".exe");
#endif

	hsc3->ClosePackfile();

	//		exeを作成
	st = 0;
#ifdef DPM_SUPPORT
	dpmc_ini( hsc3->errbuf, fname );
	st=dpmc_pack( 0 );
	if ( st ) return -st;
	st=dpmc_mkexe( type, hspexe, opt1, opt2, opt3 );
	strcat( fname, ".dpm" );
	DeleteFile( fname );
#endif
#ifdef DPM2_SUPPORT
	int myseed1,myseed2;
#ifdef HSPWIN
	myseed1 = (int)GetTickCount();	// Windowsの場合はtickをシード値とする
#else
	myseed1 = (int)time(0);			// Windows以外のランダムシード値
#endif
	myseed2 = hsp3_flength(PACKFILE);

	filepack.Reset();
	filepack.SetErrorBuffer(hsc3->errbuf);
	st = filepack.SavePackFile(fname, PACKFILE, myseed1, myseed2);
	if (st < 0) {
		return -1;
	}
	st = filepack.MakeEXEFile(type, hspexe, fname, myseed2, opt1, opt2, opt3);
	strcat(fname, ".dpm");
	DeleteFile(fname);
#endif

	//		iconins process
#ifdef ICONINS_SUPPORT
	if ((ici_use_icon+ici_use_version+ici_use_manifest+ici_use_lang+ici_use_upx)>0) {

		strcat( ici_opt, " -e\"" );
		strcat( ici_opt, ici_target );
		strcat( ici_opt, "\"" );

		if ( ici_use_icon ) {
			strcat( ici_opt," -i\"" );
			strcat( ici_opt, ici_current );
			strcat( ici_opt, ici_icon );
			strcat( ici_opt, "\"" );
		}
		if ( ici_use_version ) {
			strcat( ici_opt," -v\"" );
			strcat( ici_opt, ici_current );
			strcat( ici_opt, ici_version );
			strcat( ici_opt, "\"" );
		}
		if ( ici_use_manifest ) {
			strcat( ici_opt," -m\"" );
			strcat( ici_opt, ici_current );
			strcat( ici_opt, ici_manifest );
			strcat( ici_opt, "\"" );
		}
		if ( ici_use_lang ) {
			strcat( ici_opt," -l\"" );
			strcat( ici_opt, ici_lang );
			strcat( ici_opt, "\"" );
		}
		if ( ici_use_upx ) {
			strcat( ici_opt," -u\"" );
			strcat( ici_opt, ici_upx );
			strcat( ici_opt, "\"" );
		}
		if ( p2 ) {
			i = 1;
			while (1) {
				Sleep(100);
				CMemBuf dummy;
				int res = dummy.PutFile(ici_target);
				if (res > 0) break;
				i++;
				if (i >= 50) break;
			}
			i = WinExec( ici_opt, SW_SHOW );
			if ( i < 32 ) return -1;
		}
	}
#endif

	return -st;
}


//----------------------------------------------------------
//		Additional service on 3.0
//----------------------------------------------------------

EXPORT BOOL WINAPI hsc3_getruntime ( char *p1, char *p2, int p3, int p4 )
{
	//
	//		hsc3_getruntime val  (type5)
	//
	CharCharInt2Func delegate = get_delegate_proc<CharCharInt2Func>( "_hsc3_getruntime@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int i;
	i = hsc3->GetRuntimeFromHeader( p2, p1 );
	if ( i != 1 ) { *p1 = 0; }
	return 0;
}


EXPORT BOOL WINAPI hsc3_run ( char *p1, int p2, int p3, int p4 )
{
	//
	//		hsc3_run path, debug_flag  (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_hsc3_run@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int i;
	i = WinExec( p1, SW_SHOW );
	if ( i < 32 ) return -1;
	return 0;
}

//----------------------------------------------------------
//		Additional service on 3.1
//----------------------------------------------------------

EXPORT BOOL WINAPI aht_source( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_source var, "aht_file", "path", id (type$202)
	//		(id<0の場合は自動確保、そうでなければ指定IDに確保)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_source@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	char *p;
	int ep1;
	int res;

	int st;
	char fn[_MAX_PATH];
	char fpath[_MAX_PATH];
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	p = hei->HspFunc_prm_gets();			// パラメータ2:文字列
	strcpy( fn, p );
	p = hei->HspFunc_prm_gets();			// パラメータ3:文字列
	strcpy( fpath, p );
	ep1 = hei->HspFunc_prm_getdi( -1 );		// パラメータ4:数値

	if ( ep1 < 0 ) {
		ahtmodel =aht->AddModel();
	} else {
		ahtmodel =aht->EntryModel( ep1 );
	}

	ahtmodel->SetName( fn );
	ahtmodel->SetSource( fn );
	ahtmodel->SetSourcePath( fpath );
	res = ahtmodel->GetId();
	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入

	//		AHTを解析
	hsc3->ResetError();
	if (orgcompath==0) {
		GetModuleFileName( NULL,compath,_MAX_PATH );
		GetFilePath( compath );
		strcat( compath, fpath );
	}
	hsc3->SetCommonPath( compath );
	st = hsc3->PreProcessAht( fn, ahtmodel, 0 );

	aht->Mesf( "%s", hsc3->GetError() );

	if ( st != 0 ) {
		hsc3->PreProcessEnd();
		return st;
	}

	ahtmodel->TerminateExp();
	hsc3->PreProcessEnd();

	aht->BuildGlobalID();				// グローバルIDを更新する

	if ( st != 0 ) return st;

	return 0;
}


EXPORT BOOL WINAPI aht_ini ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_ini "prj_file" (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_ini@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	if ( aht != NULL ) { delete aht; aht=NULL; }
	aht = new CAht;
	aht->SetPrjFile( p1 );
	return 0;
}


EXPORT BOOL WINAPI aht_stdbuf ( char *p1, int p2, int p3, int p4 )
{
	//
	//		aht_stdbuf debug_buf  (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_stdbuf@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	strcpy( p1, aht->GetStdBuffer() );
	return 0;
}


EXPORT BOOL WINAPI aht_stdsize ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_stdsize var  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_stdsize@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	*p1 = (int)strlen( aht->GetStdBuffer() ) + 1;
	return 0;
}


EXPORT BOOL WINAPI aht_getopt( char *p1, char *p2, int p3, int p4 )
{
	//
	//		aht_getopt var, "parameter", modelID, maxstr (type5)
	//
	CharCharInt2Func delegate = get_delegate_proc<CharCharInt2Func>( "_aht_getopt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int max;
	char *p;
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;

	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;
	p = ahtmodel->GetAHTOption( p2 );
	max = p4;
	if ( max <= 0 ) max = 64;
	strcpy2( p1, p, max );

	return 0;
}


EXPORT BOOL WINAPI aht_getpropcnt ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getpropcnt var,modelID,sw  (type1)
	//		( sw=0:個数/1:編集行数 )
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getpropcnt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;
	ahtmodel =aht->GetModel( p2 );
	if ( ahtmodel == NULL ) return -1;

	switch( p3 ) {
	case 1:
		*p1 = ahtmodel->GetPropertyPossibleLines();
		break;
	default:
		*p1 = ahtmodel->GetPropCount();
		break;
	}
	return 0;
}


EXPORT BOOL WINAPI aht_getpropid ( int *p1, char *p2, int p3, int p4 )
{
	//
	//		aht_getpropid var,"name",modelID  (type5)
	//
	IntPtrCharInt2Func delegate = get_delegate_proc<IntPtrCharInt2Func>( "_aht_getpropid@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;
	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;
	*p1 = ahtmodel->GetPropertyID( p2 );
	return 0;
}


EXPORT BOOL WINAPI aht_getprop( char *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getprop var, DataID, propID, modelID (type1)
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_getprop@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int max;
	AHTMODEL *ahtmodel;
	AHTPROP *prop;
	char **p;
	char *t;
	if ( aht == NULL ) return -1;

	ahtmodel =aht->GetModel( p4 );
	if ( ahtmodel == NULL ) return -1;
	prop = ahtmodel->GetProperty( p3 );
	p = (char **)(&prop->name);

	max = 512;
	t = p[p2];
	if ( p2 == 2 ) t = prop->GetValue();

	strcpy2( p1, t, max );

	return (-(prop->ahttype));
}


EXPORT BOOL WINAPI aht_getproptype ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getproptype var, propID, modelID  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getproptype@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTPROP *prop;
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;
	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;
	prop = ahtmodel->GetProperty( p2 );
	*p1 = prop->ahttype;
	return 0;
}


EXPORT BOOL WINAPI aht_getpropmode ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getpropmode var, propID, modelID  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getpropmode@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTPROP *prop;
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;
	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;
	prop = ahtmodel->GetProperty( p2 );
	*p1 = prop->ahtmode;
	return 0;
}


EXPORT BOOL WINAPI aht_make ( int *p1, char *p2, int p3, int p4 )
{
	//
	//		aht_make var, "outfile", modelID, mode (type5)
	//					( mode:bit0="hsptmp"out/bit1=ahtout/bit2=HSP source build)
	//
	IntPtrCharInt2Func delegate = get_delegate_proc<IntPtrCharInt2Func>( "_aht_make@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	int st;
	int res;
	AHTMODEL *ahtmodel;
	char fname2[_MAX_PATH];

	if ( aht == NULL ) return -1;
	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;

	strcpy( fname2, ahtmodel->GetSource() );
	res = 0;
	hsc3->ResetError();
	if (orgcompath==0) {
		GetModuleFileName( NULL,compath,_MAX_PATH );
		GetFilePath( compath );
		strcat( compath, ahtmodel->GetSourcePath() );
	}
	hsc3->SetCommonPath( compath );
	st = hsc3->PreProcessAht( fname2, ahtmodel, 1 );
	aht->Mesf( "%s", hsc3->GetError() );
	if ( st != 0 ) {
		hsc3->PreProcessEnd();
		*p1 = 1;
		return st;
	}

	if ( p4&1 ) {
		res |= hsc3->SaveOutbuf( "hsptmp" );
	}
	if ( p4&2 ) {
		res |= hsc3->SaveAHTOutbuf( p2 );
	}
	if ( p4&4 ) {
		//			AHTマネージャー用にソースを構築する
		//
		aht->AddMakeBufferInit( hsc3->ahtbuf->GetBuffer(), hsc3->ahtbuf->GetSize() );
		aht->AddMakeBufferMain( hsc3->outbuf->GetBuffer(), hsc3->outbuf->GetSize() );
	}

	hsc3->PreProcessEnd();
	*p1 = res;
	if ( st != 0 ) return st;
	return 0;
}


EXPORT BOOL WINAPI aht_makeinit ( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_makeinit (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_makeinit@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	aht->InitMakeBuffer();
	return 0;
}


EXPORT BOOL WINAPI aht_makeend ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_makeend "fname" (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_makeend@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	int res;
	res = aht->SaveMakeBuffer( p1 );
	aht->DisposeMakeBuffer();
	if ( res ) return -1;
	return 0;
}


EXPORT BOOL WINAPI aht_makeput ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_makeput "message",sw (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_makeput@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	if ( p2 ) {
		aht->AddMakeBufferInit( p1 );
	} else {
		aht->AddMakeBufferMain( p1 );
	}
	return 0;
}


EXPORT BOOL WINAPI aht_setprop ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_setprop "defval", propID, modelID (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_setprop@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	AHTMODEL *ahtmodel;
	AHTPROP *prop;
	if ( aht == NULL ) return -1;

	ahtmodel =aht->GetModel( p3 );
	if ( ahtmodel == NULL ) return -1;
	prop = ahtmodel->GetProperty( p2 );
	if ( prop == NULL ) return -1;
	prop->SetNewVal( p1 );
	return 0;
}


EXPORT BOOL WINAPI aht_sendstr ( char *p1, int p2, int p3, int p4 )
{
	//		send key event (type1)
	//			aht_sendstr sendbuf, hwnd
	//
	CharInt3Func delegate = get_delegate_proc<CharInt3Func>( "_aht_sendstr@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	LPARAM lprm;
	DWORD version;
	version = GetVersion();
	if ( LOBYTE(LOWORD(version)) > 4 ) {
		_sendstr2( (HWND)p2, p1 );
		return 0;
	}
	_sendstr( (HWND)p2, p1 );

	lprm=0xd0000001;

	PostMessage( (HWND)p2, WM_KEYDOWN, VK_RETURN, lprm );
	PostMessage( (HWND)p2, WM_CHAR, VK_RETURN, lprm );
	PostMessage( (HWND)p2, WM_KEYUP, VK_RETURN, lprm );

	return 0;
}


EXPORT BOOL WINAPI aht_getmodcnt ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getmodcnt var  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getmodcnt@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	*p1 = aht->GetModelCount();
	return 0;
}


EXPORT BOOL WINAPI aht_getmodaxis ( int *p1, int p2, int p3, int p4 )
{
	//
	//		aht_getmodaxis var,modelID  (type1)
	//
	IntPtrInt3Func delegate = get_delegate_proc<IntPtrInt3Func>( "_aht_getmodaxis@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;

	ahtmodel =aht->GetModel( p2 );
	if ( ahtmodel == NULL ) return -1;

	p1[0] = ahtmodel->GetCurX();
	p1[1] = ahtmodel->GetCurY();
	p1[2] = ahtmodel->GetIconId();
	p1[3] = ahtmodel->GetPage();
	p1[4] = ahtmodel->GetNextID();
	p1[5] = ahtmodel->GetPrevID();
	return 0;
}


EXPORT BOOL WINAPI aht_setmodaxis ( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_setmodaxis modelID, x, y, page  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_setmodaxis@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	AHTMODEL *ahtmodel;
	if ( aht == NULL ) return -1;

	ahtmodel =aht->GetModel( p1 );
	if ( ahtmodel == NULL ) return -1;

	ahtmodel->SetCur( p2, p3 );
	ahtmodel->SetPage( p4 );
	return 0;
}


EXPORT BOOL WINAPI aht_prjload ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_prjload "prj_file" (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_prjload@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	int res;
	if ( aht == NULL ) return -1;
	res = aht->LoadProject( p1 );
	if ( res ) return res;
	return 0;
}


EXPORT BOOL WINAPI aht_prjsave ( BMSCR *bm, char *p1, int p2, int p3 )
{
	//
	//		aht_prjsave "prj_file" (type6)
	//
	HscIniFunc delegate = get_delegate_proc<HscIniFunc>( "_aht_prjsave@16" );
	if ( delegate != NULL ) {
		return delegate( bm, p1, p2, p3 );
	}
	int res;
	if ( aht == NULL ) return -1;
	res = aht->SaveProject( p1 );
	if ( res ) return -1;
	return 0;
}


EXPORT BOOL WINAPI aht_getprjmax( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_getprjmax var (type$202)
	//		varにmodel数を返す
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getprjmax@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	int res;

	if ( aht == NULL ) return -1;
	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数

	res = aht->GetProjectFileModelMax();
	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入
	return 0;
}


EXPORT BOOL WINAPI aht_getprjsrc( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_getprjsrc var, var2, var3, id (type$202)
	//		varにfname、var2にfpath、var3にObjectIDを返す。
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getprjsrc@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	PVal *pv2;
	APTR ap2;
	PVal *pv3;
	APTR ap3;
	int ep1;
	char *p;
	int res;

	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	ap2 = hei->HspFunc_prm_getva( &pv2 );	// パラメータ2:変数
	ap3 = hei->HspFunc_prm_getva( &pv3 );	// パラメータ3:変数
	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ4:数値

	p = aht->GetProjectFileModel( ep1 );
	hei->HspFunc_prm_setva( pv, ap, TYPE_STRING, p );	// 変数に値を代入
	p = aht->GetProjectFileModelPath( ep1 );
	hei->HspFunc_prm_setva( pv2, ap2, TYPE_STRING, p );	// 変数に値を代入
	res = aht->GetProjectFileModelID( ep1 );
	hei->HspFunc_prm_setva( pv3, ap3, TYPE_INUM, &res );	// 変数に値を代入

	return 0;
}


EXPORT BOOL WINAPI aht_prjload2( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_prjload2 model_id, id (type$202)
	//		( モデルデータの更新。aht_prjloadの後にモデルごとに実行する。 )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_prjload2@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	int ep1,ep2;
	int res;

	if ( aht == NULL ) return -1;

	res = 0;
	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ1:数値
	ep2 = hei->HspFunc_prm_getdi( 0 );		// パラメータ2:数値

	aht->LoadProjectApply( ep1, ep2 );

	return res;
}


EXPORT BOOL WINAPI aht_prjloade( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_prjloade (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_prjloade@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	if ( aht == NULL ) return -1;

	aht->LoadProjectEnd();
	return 0;
}


EXPORT BOOL WINAPI aht_delmod( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_delmod modelID  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_delmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	aht->DeleteModel( p1 );
	return 0;
}


EXPORT BOOL WINAPI aht_linkmod( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_linkmod modelID, NextID  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_linkmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	aht->LinkModel( p1, p2 );
	return 0;
}


EXPORT BOOL WINAPI aht_unlinkmod( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_unlinkmod modelID  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_unlinkmod@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	aht->UnlinkModel( p1 );
	return 0;
}


EXPORT BOOL WINAPI aht_setpage( int p1, int p2, int p3, int p4 )
{
	//
	//		aht_setpage cur,max  (type0)
	//
	FourIntFunc delegate = get_delegate_proc<FourIntFunc>( "_aht_setpage@16" );
	if ( delegate != NULL ) {
		return delegate( p1, p2, p3, p4 );
	}
	if ( aht == NULL ) return -1;
	aht->SetPage( p1, p2 );
	return 0;
}


EXPORT BOOL WINAPI aht_getpage( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_getpage var,var2 (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getpage@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	PVal *pv2;
	APTR ap2;
	int res;

	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	ap2 = hei->HspFunc_prm_getva( &pv2 );	// パラメータ2:変数

	res = aht->GetCurrentPage();
	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入
	res = aht->GetMaxPage();
	hei->HspFunc_prm_setva( pv2, ap2, TYPE_INUM, &res );	// 変数に値を代入

	return 0;
}


EXPORT BOOL WINAPI aht_propupdate( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_propupdate model_id (type$202)
	//		( プロパティの更新 )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_propupdate@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	int ep1;
	if ( aht == NULL ) return -1;
	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ1:数値
	aht->UpdateModelProperty( ep1 );
	return 0;
}


EXPORT BOOL WINAPI aht_parts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_parts "path","list" (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_parts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	char *ep1;
	char *ep2;
	char path[256];
	if ( aht == NULL ) return -1;
	ep1 = hei->HspFunc_prm_gets();			// パラメータ1:文字列
	strcpy( path, ep1 );
	ep2 = hei->HspFunc_prm_gets();			// パラメータ2:文字列
	aht->BuildParts( ep2, path );
	return 0;
}


EXPORT BOOL WINAPI aht_getparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_getparts id, var,var2,var3 (type$202)
	//					 ( ICONID,name,classnameが代入される )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	PVal *pv2;
	APTR ap2;
	PVal *pv3;
	APTR ap3;
	int ep1;
	int res;
	char *p;

	if ( aht == NULL ) return -1;

	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ1:数値
	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ2:変数
	ap2 = hei->HspFunc_prm_getva( &pv2 );	// パラメータ3:変数
	ap3 = hei->HspFunc_prm_getva( &pv3 );	// パラメータ4:変数

	res = aht->GetPartsIconID(ep1);
	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入
	p = aht->GetPartsName(ep1);
	hei->HspFunc_prm_setva( pv2, ap2, TYPE_STRING, p );	// 変数に値を代入
	p = aht->GetPartsClassName(ep1);
	hei->HspFunc_prm_setva( pv3, ap3, TYPE_STRING, p );	// 変数に値を代入

	return 0;
}


EXPORT BOOL WINAPI aht_listparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_listparts var,"clsname" (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_listparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	char *ep1;
	char *p;

	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	ep1 = hei->HspFunc_prm_gets();			// パラメータ2:文字列
	p = aht->SearchModelByClassName( ep1 );
	hei->HspFunc_prm_setva( pv, ap, TYPE_STRING, p );	// 変数に値を代入
	return 0;
}


EXPORT BOOL WINAPI aht_findstart( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_findstart var (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findstart@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	if ( aht == NULL ) return -1;
	aht->FindModelStart();
	homeid = -1;
	ahtbuild_error = 0;
	return 0;
}


EXPORT BOOL WINAPI aht_findparts( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_findparts var (type$202)
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findparts@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	int res;
	AHTMODEL *m;
	char *p;
	int len;
	int i;
	int statval;

	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	res = aht->FindModel();
	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入

	if ( res >= 0 ) {
		m = aht->GetModel( res );
		p = m->GetClass();
		len = (int)strlen( p ) - 5; if ( len < 0 ) len = 0;
		if ( strcmp( p+len, ".home" )==0 ) {		// homeクラスかどうか確認する
			if ( homeid != -1 ) ahtbuild_error = 2;
			homeid = res;
		}
	}

	statval = 0;
	i = aht->FindModelGetParentId();				// リンク元IDを取得する
	if ( i >= 0 ) {
		m = aht->GetModel( i );
		p = m->GetClass();
		len = (int)strlen( p ) - 8; if ( len < 0 ) len = 0;
		if ( strcmp( p+len, ".routine" )==0 ) {		// routineクラスかどうか確認する
			statval = -1;
		}
	}

	return statval;
}


EXPORT BOOL WINAPI aht_findend( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_findend var, mode (type$202)
	//		( mode=0:ERROR表示あり/1:なし )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_findend@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	int ep1;
	char *err;

	if ( aht == NULL ) return -1;
	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ2:数値

	if ( homeid == -1 ) ahtbuild_error = 1;

	if ( ep1 == 0 ) {
		err = NULL;
		switch( ahtbuild_error ) {
		case 1:
			err = "ホームのパーツが配置されていません。\n最初に実行するホームを置いてください。";
			break;
		case 2:
			err = "ホームのパーツが複数配置されています。\nホームは１つだけ置いてください。";
			break;
		default:
			break;
		}
		if ( err != NULL ) Alert( err );
	}

	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &ahtbuild_error );	// 変数に値を代入

	return 0;
}


EXPORT BOOL WINAPI aht_getexid( HSPEXINFO *hei, int p1, int p2, int p3 )
{
	//
	//		aht_getexid var, mode (type$202)
	//			( varにモデルIDを代入する )
	//			( mode:0=リンク元ID/1=HOMEのID )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_aht_getexid@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	int ep1;
	int res;

	if ( aht == NULL ) return -1;

	ap = hei->HspFunc_prm_getva( &pv );		// パラメータ1:変数
	ep1 = hei->HspFunc_prm_getdi( 0 );		// パラメータ2:数値

	res = -1;
	switch( ep1 ) {
	case 0:
		res = aht->FindModelGetParentId();
		break;
	case 1:
		res = homeid;
		break;
	default:
		break;
	}

	hei->HspFunc_prm_setva( pv, ap, TYPE_INUM, &res );	// 変数に値を代入

	return 0;
}


//----------------------------------------------------------

#ifdef USE_HSMANAGER

EXPORT BOOL WINAPI hman_init(HSPEXINFO *hei, int p1, int p2, int p3)
{
	//
	//		hman_init "pathname", mode (type$202)
	//			( mode:未使用 )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_init@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	char *ep1;
	int ep2;
	int res;
	char path[256];

	ep1 = hei->HspFunc_prm_gets();			// パラメータ1:文字列
	strncpy(path, ep1, 255);
	ep2 = hei->HspFunc_prm_getdi(0);		// パラメータ2:数値

	res = hsman.initalize( path );
	if (res < 0) return -1;

	return 0;
}

EXPORT BOOL WINAPI hman_search(HSPEXINFO *hei, int p1, int p2, int p3)
{
	//
	//		hman_search "keyword" (type$202)
	//			( 文字列を指定してヘルプを検索する )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_search@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	char key[256];
	char *ep1;
	int res;

	ep1 = hei->HspFunc_prm_gets();			// パラメータ1:文字列
	strncpy(key, ep1, 255);

	res = hsman.searchIndex(key);
	if (res < 0) return -1;

	return 0;
}

EXPORT BOOL WINAPI hman_getresult(HSPEXINFO *hei, int p1, int p2, int p3)
{
	//
	//		hman_getresult var,option (type$202)
	//			( varに結果文字列を代入する )
	//
	HspexInt3Func delegate = get_delegate_proc<HspexInt3Func>( "_hman_getresult@16" );
	if ( delegate != NULL ) {
		return delegate( hei, p1, p2, p3 );
	}
	PVal *pv;
	APTR ap;
	char *res;
	int ep1;

	ap = hei->HspFunc_prm_getva(&pv);		// パラメータ1:変数
	ep1 = hei->HspFunc_prm_getdi(0);		// パラメータ2:数値
	res = hsman.getMessage();
	hei->HspFunc_prm_setva(pv, ap, TYPE_STRING, res);	// 変数に値を代入

	return 0;
}

#endif

//----------------------------------------------------------
