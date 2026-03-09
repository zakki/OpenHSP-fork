//
//	HSPCC : HSP Code Compiler Manager
//				onion software 2002/12
//
#ifdef HSPWIN
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <memory>

#if defined(HSPLINUX)
#include <unistd.h>
#endif

#ifdef HSPLINUX
#include <sys/stat.h>
#endif

#ifdef HSPWIN
#include <direct.h>
#endif
#include <string.h>
#include <ctype.h>

#include "../hsp3/hsp3config.h"
#include "supio.h"

#include "membuf.h"
#include "chsp/chsp_libtcc_shared.h"
#include "chsp/chsp_frontend_v2.h"
#include "hsc3.h"
#include "token.h"
#include "hsmanager.h"

/*----------------------------------------------------------*/

enum class ChspNativeCompileMode
{
	None,
	Libtcc,
};

static void usage1( void )
{
static 	char *p[] = {
	"usage: hspcmp [options] [filename]",
	"       -o??? set output file to ???",
	"       -d    add debug information",
	"       -p    preprocessor only",
	"       -t    preprocessor and chsp transform only",
	"       -c    HSP2.55 compatible mode",
	"       -i    input UTF-8 source code",
	"       -u    output UTF-8 strings",
	"       -w    force debug window on",
	"       -e?   execute/view .ax runtime",
	"       -r    execute runtime with result",
	"       -s    output string map",
	"       -m    compile for Emscripten",
	"       ---------------------------------",
	"       -h??? print command help",
	"       -lk???  print HSP3 keyword list",
	"       -ll???  print source label list",
	"       -lv???  print source variable list",
	"       -ls???  print source keyword list",
	"       -lr     include reference list",
	"       -lp     allow partial matches.",
	"       ---------------------------------",
	"       --syspath=??? set system folder for execute",
	"       --compath=??? set common path to ???",
	"       --chsp-target=c|cpp set cHSP native output target (default: c)",
	"       --chsp-compile=libtcc|none set cHSP native compile mode (default: libtcc)",
	NULL };
	int i;
	for(i=0; p[i]; i++)
		printf( "%s\n", p[i]);
}

static int has_extension( char *path, const char *ext )
{
	char *dot = strrchr( path, '.' );
	if ( dot == NULL ) return 0;
	if ( strcmp( dot, ext ) == 0 ) return 1;
	return 0;
}

static int contains_chsp_directive( char *text )
{
	if ( text == NULL ) return 0;
	if ( strstr( text, "#chsp_" ) != NULL ) return 1;
	return 0;
}

/*----------------------------------------------------------*/

int main( int argc, char *argv[] )
{
	char a1,a2,a3;
	int b,st;
	int cmpopt,ppopt,utfopt,pponly,chsp_transform_only,execobj,strmap,hsphelp;
	char *opt_lk = NULL;
	char *opt_ls = NULL;
	int opt_lsref, opt_lsmode;
	char fname[HSP_MAX_PATH];
	char fname2[HSP_MAX_PATH];
	char fname_chi[HSP_MAX_PATH];
	char fname_cpp[HSP_MAX_PATH];
	char oname[HSP_MAX_PATH];
	char compath[HSP_MAX_PATH];
	char syspath[HSP_MAX_PATH];
	char helpkey[256];
	ChspNativeTarget chsp_target;
	ChspNativeCompileMode chsp_compile_mode;
	CHsc3 *hsc3=NULL;

	//	check switch and prm

	if (argc<2) { usage1();return -1; }

	st = 0; ppopt = 0; cmpopt = 0; utfopt = 0; pponly = 0; chsp_transform_only = 0; strmap = 0; hsphelp = 0; opt_lsref = 0; opt_lsmode = 0;
	execobj = 0;
	fname[0]=0;
	fname2[0]=0;
	fname_chi[0]=0;
	fname_cpp[0]=0;
	oname[0]=0;
	syspath[0]=0;
	helpkey[0] = 0;
	chsp_target = ChspNativeTarget::C;
	chsp_compile_mode = ChspNativeCompileMode::Libtcc;

#ifdef HSPLINUX
	strcpy( compath,"common/" );
#else
	strcpy( compath,"common\\" );
#endif

	for (b=1;b<argc;b++) {
		a1=*argv[b];a2=tolower(*(argv[b]+1));
#ifdef HSPLINUX
		if (a1!='-') {
#else
		if ((a1!='/')&&(a1!='-')) {
#endif
			strcpy(fname,argv[b]);
		} else {
			a3=tolower(*(argv[b]+2));
			if (strncmp(argv[b], "--compath=", 10) == 0) {
				strcpy( compath, argv[b] + 10 );
				continue;
			}
			if (strncmp(argv[b], "--syspath=", 10) == 0) {
				strcpy( syspath, argv[b] + 10 );
				continue;
			}
			if (strncmp(argv[b], "--chsp-target=", 14) == 0) {
				const char *value = argv[b] + 14;
				if ( strcmp( value, "c" ) == 0 ) {
					chsp_target = ChspNativeTarget::C;
					continue;
				}
				if ( strcmp( value, "cpp" ) == 0 ) {
					chsp_target = ChspNativeTarget::Cpp;
					chsp_compile_mode = ChspNativeCompileMode::None;
					continue;
				}
				printf( "Invalid cHSP target selected.\n" );
				return 1;
			}
			if (strncmp(argv[b], "--chsp-compile=", 15) == 0) {
				const char *value = argv[b] + 15;
				if ( strcmp( value, "libtcc" ) == 0 ) {
					chsp_compile_mode = ChspNativeCompileMode::Libtcc;
					continue;
				}
				if ( strcmp( value, "none" ) == 0 ) {
					chsp_compile_mode = ChspNativeCompileMode::None;
					continue;
				}
				printf( "Invalid cHSP compile mode selected.\n" );
				return 1;
			}
			switch (a2) {
			case 'c':
				ppopt |= HSC3_OPT_NOHSPDEF; break;
			case 'p':
				pponly=1; break;
			case 't':
				chsp_transform_only=1; break;
			case 'd':
				ppopt |= HSC3_OPT_DEBUGMODE; cmpopt|=HSC3_MODE_DEBUG; break;
			case 'i':
				ppopt |= HSC3_OPT_UTF8IN; utfopt=1; cmpopt|=HSC3_MODE_UTF8; break;
			case 'u':
				utfopt = 1; cmpopt |= HSC3_MODE_UTF8; break;
			case 'j':
				ppopt |= HSC3_OPT_UTF8IN; utfopt = 1; break;
			case 'w':
				cmpopt|=HSC3_MODE_DEBUGWIN; break;
			case 's':
				strmap = 1; cmpopt |= HSC3_MODE_STRMAP; break;
			case 'm':
				ppopt |= HSC3_OPT_EMSCRIPTEN; break;
			case 'o':
				strcpy(oname, argv[b] + 2);
				break;
			case 'e':
				execobj = 1;
				if ( a3=='0' ) execobj|=8;
				break;
			case 'r':
				execobj = 2;
				if ( a3=='0' ) execobj|=8;
				break;
			case 'h':
				hsphelp = 1;
				strcpy(helpkey, argv[b] + 2);
				break;
			case 'l':
				if (a3 == 'k') {
					opt_lk = argv[b] + 3; break;
				}
				if (a3 == 'l') {
					opt_lsmode = 0;
					opt_ls = argv[b] + 3; break;
				}
				if (a3 == 'v') {
					opt_lsmode = 1;
					opt_ls = argv[b] + 3; break;
				}
				if (a3 == 's') {
					opt_lsmode = 2;
					opt_ls = argv[b] + 3; break;
				}
				if (a3 == 'r') {
					opt_lsref = 16; break;
				}
				if (a3 == 'p') {
					opt_lsref = 32; break;
				}
				st = 1;
				break;
			default:
				st = 1;break;
			}
		}
	}

	if (st) { printf("Illegal switch selected.\n");return 1; }

	//		help main
	if (hsphelp) {
		int res;
		HspHelpManager hman;
		strcat(syspath, "hsphelp");
		res = hman.initalize(syspath);
		if (res == 0) {
			res = hman.searchIndex(helpkey);
		}
		puts(hman.getMessage());
		return res;
	}

	hsc3 = new CHsc3;
	hsc3->SetCommonPath(compath);

	//		keyword main
	if (opt_lk) {
		if (*opt_lk == 0) opt_lk = NULL;
		st = hsc3->GetCmdList(2, opt_lk);
		puts(hsc3->GetError());
		delete hsc3;
		return st;
	}

	if (fname[0]==0) { printf("No file name selected.\n");return 1; }
	if ((pponly != 0) && (chsp_transform_only != 0)) {
		printf("Options -p and -t cannot be used together.\n");
		delete hsc3;
		return 1;
	}
	if ( chsp_compile_mode != ChspNativeCompileMode::None && chsp_target != ChspNativeTarget::C ) {
		printf("cHSP native compilation currently requires --chsp-target=c.\n");
		delete hsc3;
		return 1;
	}
#if !defined(HSPLINUX) && !defined(HSPWIN)
	if ( chsp_compile_mode == ChspNativeCompileMode::Libtcc ) {
		printf("libtcc native compilation is currently supported only on Linux and Win32.\n");
		delete hsc3;
		return 1;
	}
#endif

	if (oname[0]==0) {
		strcpy( oname,fname ); cutext( oname );
		if (strmap) {
			addext(oname, "strmap");
		}
		else {
			addext(oname, "ax");
		}
	}
	strcpy( fname2, fname ); cutext( fname2 ); addext( fname2,"i" );
	strcpy( fname_chi, fname ); cutext( fname_chi ); addext( fname_chi,"chi" );
	strcpy( fname_cpp, fname ); cutext( fname_cpp ); addext( fname_cpp, chsp_target == ChspNativeTarget::C ? "c" : "cpp" );
	if (( has_extension( fname, ".chsp" ) == 0 )&&( has_extension( fname, ".hsp" ) == 0 )) {
		addext( fname,"hsp" );			// 拡張子がなければ追加する
	}

	//		label pick
	if (opt_ls) {
		if (*opt_ls == 0) opt_ls = NULL;

		//		通常のコンパイル
		hsc3->InitAnalysisInfo(opt_lsmode | opt_lsref, opt_ls);
		st = hsc3->PreProcess(fname, fname2, ppopt, fname);
		if ((pponly == 0) && (st == 0)) {
			st = hsc3->CompileLabelOut(fname2, cmpopt);
		}
		if (st >= 0) {
			puts(hsc3->GetAnalysisInfo());
		}
		else {
			printf("No match.\n");
		}
		hsc3->DeleteAnalysisInfo();
		hsc3->PreProcessEnd();
		delete hsc3;
		return st;
	}

	//		call main

	if ( execobj ) {
		//		ランタイムを起動
		char execmd[4096];
		st = hsc3->GetRuntimeFromHeader( fname, oname );
		if ( st != 1 ) {
			strcpy( oname, "hsp3.exe" );			// デフォルトランタイム
		}

#ifdef HSPLINUX
		cutext( oname );
		if ( execobj & 8 ) {
			printf("Runtime[%s].\n",oname);
		} else {
			int result;
			printf("Execute from %s runtime[%s](%d).\n",fname,oname,execobj);
			sprintf(execmd,"%s./%s %s",syspath,oname,fname);
			//sprintf(execmd,"%s./%s %s >%s.hspres",syspath,oname,fname,syspath);
			
			result = system(execmd);
			if ( WIFEXITED(result) ) {
				result = WEXITSTATUS(result);
				printf("hsed: Process end %d.\n",result);
				if ( execobj==2 ) {
					if ( result != 0 ) {			// エラーがあった時
						while(1) {
							result = getchar();
							if (( result == 13 )||( result == 10 )) break;
						}
					}
				}
			} else {
				printf("hsed: Process error.\n");
			}
		}
#else
		if ( execobj & 8 ) {
			printf("Runtime[%s].\n",oname);
		} else {
			sprintf( execmd, "%s %s", oname, fname );
			st = WinExec( execmd, SW_SHOW );
			if ( st < 32 ) {
				printf("Runtime file missing.\n");
			}
		}
#endif

	} else {
		//		通常のコンパイル
		st = hsc3->PreProcess( fname, fname2, ppopt, fname );
		if (( pponly == 0 )&&( st == 0 )) {
			std::shared_ptr<CMemBuf> frontend_errbuf( hsc3->errbuf, []( CMemBuf * ) {} );
			CChspFrontendV2 frontend( frontend_errbuf );
			CMemBuf transformed_out;
			CMemBuf cpp_out;
			char *preprocessed = hsc3->outbuf != NULL ? hsc3->outbuf->GetBuffer() : NULL;
				int has_chsp = contains_chsp_directive( preprocessed );
				st = frontend.GenerateFromBuffer( fname, preprocessed != NULL ? preprocessed : "", &transformed_out, &cpp_out, chsp_target );
				if (( st == 0 )&&( has_chsp || ( chsp_transform_only != 0 ) )) {
					if ( cpp_out.SaveFile( fname_cpp ) < 0 ) {
						hsc3->Print( (char *)"#Can't write generated cHSP native file." );
						st = -1;
					}
				}
				if (( st == 0 )&&( has_chsp )&&( chsp_transform_only == 0 )&&( chsp_compile_mode == ChspNativeCompileMode::Libtcc )) {
#ifdef CHSP_HAS_LIBTCC
					st = chsp_compile_library_with_libtcc( hsc3, fname_cpp, compath, transformed_out );
#endif
				}
			if (( st == 0 )&&( chsp_transform_only != 0 )) {
				if ( transformed_out.SaveFile( fname_chi ) < 0 ) {
					hsc3->Print( (char *)"#Can't write generated cHSP transform file." );
					st = -1;
				}
			} else if ( st == 0 ) {
				CMemBuf *next_outbuf = new CMemBuf( transformed_out.GetSize() + 1 );
				if ( transformed_out.GetSize() > 0 ) {
					next_outbuf->PutData( transformed_out.GetBuffer(), transformed_out.GetSize() );
				}
				next_outbuf->Put( (char)0 );
				delete hsc3->outbuf;
				hsc3->outbuf = next_outbuf;
			}
		}
		if (( pponly == 0 )&&( chsp_transform_only == 0 )&&( st == 0 )) {
			st = hsc3->Compile( fname2, oname, cmpopt );
		}
		puts( hsc3->GetError() );
		hsc3->PreProcessEnd();
	}

	if ( hsc3 != NULL ) { delete hsc3; hsc3=NULL; }
	return st;
}
