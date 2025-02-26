/*--------------------------------------------------------
	HSP3 interpreter main
									  1995/10 onitama
									  1997/7  onitama
									  1999/8  onitama
									  2003/4  onitama
  --------------------------------------------------------*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <string>
#include "emscripten.h"
#include "hsp3dish.h"

/*----------------------------------------------------------*/

static int st;

static int start_hsp3dish(char *startfile)
{
	int res = hsp3dish_init( startfile );
	if ( res ) return res;

	hsp3dish_option( st );
	res = hsp3dish_exec();

	return res;
}

static void dpm_onload(const char *message)
{
	printf("ダウンロードしました(%s)\n", message);
	start_hsp3dish(nullptr);
}

static void dpm_onerror(const char *message)
{
	printf("ダウンロード中にエラーが発生しました(%s)\n", message);
}


int main( int argc, char *argv[] )
{
	int res;
	char *p;

// #ifdef HSPDEBUG
#if 1
	char a1,a2,a3;
	int b,index;
	char mydir[1024];
	char *cl;
	std::string clopt;
	std::string clmod;

	p = "";
	st = 0;
	index = 0;
	cl = argv[0];
	if (*cl=='/') {
		clmod = cl;
	} else {
		getcwd( mydir, 1024 );
		clmod = mydir;
		clmod += "/";
		clmod += cl;
	}

	for (b=1;b<argc;b++) {
		a1=*argv[b];a2=tolower(*(argv[b]+1));
#ifdef HSPLINUX
		if (a1!='-') {
#else
		if ((a1!='/')&&(a1!='-')) {
#endif
			if ( index == 0 ) {
				p = argv[b];
			} else {
				if ( index > 1 ) clopt+=" ";
				clopt += argv[b];
			}
			index++;
		} else {
			switch (a2) {
			default:
				printf("Illegal switch selected.\n");
				break;
			}
		}
	}
#else
	p = NULL;
#endif

	hsp3dish_cmdline((char *)clopt.c_str());
	hsp3dish_modname((char *)clmod.c_str());


	char *env_download_dpm = getenv( "HSP_DOWNLOAD_DPM" );
	if ( env_download_dpm ) {
		emscripten_async_wget(
			env_download_dpm,
			"data.dpm",
			dpm_onload,
			dpm_onerror);
		return 0;
	}

	start_hsp3dish(p);
	return res;
}
