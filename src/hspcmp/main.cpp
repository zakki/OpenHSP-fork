
//
//	HSPCC : HSP Code Compiler Manager
//				onion software 2002/12
//
#ifdef HSPWIN
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#ifdef HSPLINUX
#include <unistd.h>
#endif

#ifdef HSPWIN
#include <direct.h>
#endif
#include <string.h>
#include <ctype.h>
#ifdef HSPWIN
#include <string>
#include <vector>
#include "../hsp3/hsp3pathio.h"
#endif

#include "../hsp3/hsp3config.h"
#include "supio.h"

#include "hsc3.h"
#include "token.h"
#include "hsmanager.h"

static int copy_command_path(char* destination, size_t destination_size, const char* source)
{
	size_t length;
	if (destination == NULL || source == NULL || destination_size == 0) return -1;
	length = strlen(source);
	if (length >= destination_size) return -1;
	memcpy(destination, source, length + 1);
	return 0;
}

static int append_command_text(char* destination, size_t destination_size, const char* suffix)
{
	size_t destination_length;
	size_t suffix_length;
	if (destination == NULL || suffix == NULL || destination_size == 0) return -1;
	destination_length = strlen(destination);
	suffix_length = strlen(suffix);
	if (destination_length >= destination_size || suffix_length >= destination_size - destination_length) return -1;
	memcpy(destination + destination_length, suffix, suffix_length + 1);
	return 0;
}

/*----------------------------------------------------------*/

static void usage1( void )
{
static 	char *p[] = {
	"usage: hspcmp [options] [filename]",
	"       -o??? set output file to ???",
	"       -d    add debug information",
	"       -p    preprocessor only",
	"       -c    HSP2.55 compatible mode",
	"       -i    input UTF-8 source code",
	"       -u    output UTF-8 strings",
	"       -w    force debug window on",
	"       -e?   execute/view .ax runtime",
	"       -r    execute runtime with result",
	"       -s    output string map",
	"       -x    select HSP32 runtime",
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
	NULL };
	int i;
	for(i=0; p[i]; i++)
		printf( "%s\n", p[i]);
}

/*----------------------------------------------------------*/

#ifdef HSPWIN
int wmain( int argc, wchar_t *argv[] )
#else
int main( int argc, char *argv[] )
#endif
{
	char a1,a2,a3;
	int b,st;
	int cmpopt,ppopt,utfopt,pponly,execobj,strmap,hsphelp,hsp64;
	char *opt_lk = NULL;
	char *opt_ls = NULL;
	int opt_lsref, opt_lsmode;
	char fname[HSP_MAX_PATH];
	char fname2[HSP_MAX_PATH];
	char oname[HSP_MAX_PATH];
	char compath[HSP_MAX_PATH];
	char syspath[HSP_MAX_PATH];
	char helpkey[256];
	CHsc3 *hsc3=NULL;

#ifdef HSPWIN
	std::vector<std::string> utf8_args;
	utf8_args.reserve(argc);
	for (int i = 0; i < argc; ++i) {
		char* converted = hsp_utf8_from_wide(argv[i]);
		if (converted == NULL) return 1;
		utf8_args.push_back(converted);
		free(converted);
	}
#endif

	//	check switch and prm

	if (argc<2) { usage1();return -1; }

	st = 0; ppopt = 0; cmpopt = 0; utfopt = 0; pponly = 0; strmap = 0; hsphelp = 0; opt_lsref = 0; opt_lsmode = 0; hsp64 = 1;
	execobj = 0;
	fname[0]=0;
	fname2[0]=0;
	oname[0]=0;
	syspath[0]=0;
	helpkey[0] = 0;

#ifdef HSPLINUX
	strcpy( compath,"common/" );
#else
	strcpy( compath,"common\\" );
#endif

	for (b=1;b<argc;b++) {
#ifdef HSPWIN
			const char* arg = utf8_args[b].c_str();
#else
			const char* arg = argv[b];
#endif
		a1=*arg;a2=tolower(*(arg+1));
#ifdef HSPLINUX
		if (a1!='-') {
#else
		if ((a1!='/')&&(a1!='-')) {
#endif
			if (copy_command_path(fname, sizeof(fname), arg) != 0) {
				printf("Source path is too long.\n");
				return 1;
			}
		} else {
			a3=tolower(*(arg+2));
			if (strncmp(arg, "--compath=", 10) == 0) {
				if (copy_command_path(compath, sizeof(compath), arg + 10) != 0) {
					printf("Common path is too long.\n");
					return 1;
				}
				continue;
			}
			if (strncmp(arg, "--syspath=", 10) == 0) {
				if (copy_command_path(syspath, sizeof(syspath), arg + 10) != 0) {
					printf("System path is too long.\n");
					return 1;
				}
				continue;
			}
			switch (a2) {
			case 'c':
				ppopt |= HSC3_OPT_NOHSPDEF; break;
			case 'p':
				pponly=1; break;
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
				if (copy_command_path(oname, sizeof(oname), arg + 2) != 0) {
					printf("Output path is too long.\n");
					return 1;
				}
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
				strcpy(helpkey, arg + 2);
				break;
			case 'l':
				if (a3 == 'k') {
					opt_lk = (char*)arg + 3; break;
				}
				if (a3 == 'l') {
					opt_lsmode = 0;
					opt_ls = (char*)arg + 3; break;
				}
				if (a3 == 'v') {
					opt_lsmode = 1;
					opt_ls = (char*)arg + 3; break;
				}
				if (a3 == 's') {
					opt_lsmode = 2;
					opt_ls = (char*)arg + 3; break;
				}
				if (a3 == 'r') {
					opt_lsref = 16; break;
				}
				if (a3 == 'p') {
					opt_lsref = 32; break;
				}
				st = 1;
				break;
			case 'x':
				hsp64 = 0; break;
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
		if (append_command_text(syspath, sizeof(syspath), "hsphelp") != 0) {
			printf("Help path is too long.\n");
			return 1;
		}
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

	if (oname[0]==0) {
		if (copy_command_path(oname, sizeof(oname), fname) != 0) return 1;
		cutext( oname );
		if (strmap) {
			if (strlen(oname) + strlen("strmap") + 2 > sizeof(oname)) return 1;
			addext(oname, "strmap");
		}
		else {
			if (strlen(oname) + strlen("ax") + 2 > sizeof(oname)) return 1;
			addext(oname, "ax");
		}
	}
	if (copy_command_path(fname2, sizeof(fname2), fname) != 0) return 1;
	cutext( fname2 );
	if (strlen(fname2) + 2 >= sizeof(fname2)) return 1;
	addext( fname2,"i" );
	if (strlen(fname) + strlen("hsp") + 2 > sizeof(fname)) return 1;
	addext( fname,"hsp" );			// 拡張子がなければ追加する

	//		HSP64 check
	if (hsp64) {
		ppopt |= HSC3_OPT_RUNTIME64 | HSC3_OPT_UTF8OUT;
	}

	//		label pick
	if (opt_ls) {
		if (*opt_ls == 0) opt_ls = NULL;

		//		通常のコンパイル
		hsc3->InitAnalysisInfo(opt_lsmode | opt_lsref, opt_ls);
		st = hsc3->PreProcess(fname, fname2, ppopt, fname);
		if ((pponly == 0) && (st == 0)) {
			if (hsp64) {
				if (hsc3->GetHeaderOption() & HEDINFO_HSP64) {
					cmpopt |= HSC3_MODE_RUNTIME64 | HSC3_MODE_UTF8;
				}
			}
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

#if defined(HSPLINUX)||defined(HSPMAC)
		cutext( oname );
		if ( execobj & 8 ) {
			printf("Runtime[%s].\n",oname);
		} else {
			int result;
			int command_length;
			printf("Execute from %s runtime[%s](%d).\n",fname,oname,execobj);
			command_length = snprintf(execmd, sizeof(execmd), "%s./%s %s", syspath, oname, fname);
			if (command_length < 0 || (size_t)command_length >= sizeof(execmd)) {
				printf("Runtime command is too long.\n");
				return 1;
			}
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
			int command_length = snprintf(execmd, sizeof(execmd), "%s %s", oname, fname);
			if (command_length < 0 || (size_t)command_length >= sizeof(execmd)) {
				printf("Runtime command is too long.\n");
				st = -1;
			}
			else {
				st = hsp_exec_utf8( execmd );
			}
			if ( st < 32 ) {
				printf("Runtime file missing.\n");
			}
		}
#endif

	} else {
		//		通常のコンパイル
		st = hsc3->PreProcess( fname, fname2, ppopt, fname );
		if (( pponly == 0 )&&( st == 0 )) {
			if (hsp64) {
				if (hsc3->GetHeaderOption() & HEDINFO_HSP64) {
					cmpopt |= HSC3_MODE_RUNTIME64 | HSC3_MODE_UTF8;
				}
			}
			st = hsc3->Compile( fname2, oname, cmpopt );
		}
		puts( hsc3->GetError() );
		hsc3->PreProcessEnd();
	}

	if ( hsc3 != NULL ) { delete hsc3; hsc3=NULL; }
	return st;
}
