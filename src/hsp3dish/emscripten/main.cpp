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
#include <vector>
#include "emscripten.h"
#include "hsp3dish.h"
#include "../supio.h"

/*----------------------------------------------------------*/

static int st;

static int start_hsp3dish( char *startfile )
{
	printf("Call hsp3dish_init\n");
	int res = hsp3dish_init( startfile );
	printf("hsp3dish_init returned %d\n", res);
	if ( res ) return res;

	hsp3dish_option( st );
	printf("Start hsp3dish_exec\n" );
	res = hsp3dish_exec();
	printf("hsp3dish_exec returned %d\n", res);

	return res;
}

extern std::vector<int> renderResult;
extern int __windowSize[2];
void main_loop( void )
{
	int w = __windowSize[0], h = __windowSize[1];
	if (renderResult.empty())
	{
		printf("renderResult is NULL, allocating memory for %d x %d\n", w, h);
		return;
	}
	if (w <= 0 || h <= 0)
	{
		printf("Invalid window size: %d x %d\n", w, h);
		return;
	}
	// printf("main_loop called %d %d %x %x \n", w, h, renderResult[0], renderResult[1]);
	EM_ASM_({
		let data = Module.HEAPU8.slice($0, $0 + $1 * $2 * 4);
		let context = Module['canvas'].getContext('2d');
		let imageData = context.getImageData(0, 0, $1, $2);
		imageData.data.set(data);
		context.putImageData(imageData, 0, 0);
	}, renderResult.data(), w, h);
}

static volatile int download_complete = 0;
static void dpm_onload( const char *message )
{
	Alertf( "Downloaded successfully (%s)\n", message );
	download_complete = 1;

	// start_hsp3dish( nullptr );

	pthread_t thread;
	if ( pthread_create( &thread, nullptr, (void *(*)(void *))start_hsp3dish, nullptr ) != 0 ) {
		Alertf( "Failed to create thread for hsp3dish.\n" );
		download_complete = -1;
		return;
	}

	char *env_fps = getenv( "HSP_FPS" );
	int hsp_fps = 0;
	if ( env_fps ) {
		hsp_fps = atoi( env_fps );
	}
	emscripten_set_main_loop(main_loop, hsp_fps, 1);
}

static void dpm_onerror( const char *message )
{
	Alertf( "Error during download (%s)\n", message );
	download_complete = -1;
}


int main( int argc, char *argv[] )
//int __main_void()
{
//	int argc = 1;
//	char *argv[] = { (char *)"hsp3dish", NULL };

	int res;
	char *p;
	printf("HSP3Dish Emscripten version\n");

#ifdef HSPDEBUG
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


	char *env_dpm_path = getenv( "HSP_DPM_PATH" );
	if ( env_dpm_path ) {
		printf("Downloading DPM from %s\n", env_dpm_path);
		emscripten_async_wget(
			env_dpm_path,
			"data.dpm",
			dpm_onload,
			dpm_onerror);
		return 0;
	}

	printf("No HSP_DPM_PATH environment variable found.\n");

	start_hsp3dish(p);
	return res;
}
