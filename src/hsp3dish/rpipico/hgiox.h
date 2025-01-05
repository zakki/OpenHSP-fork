//
//	hgiox.cpp header
//
#ifndef __hgiox_h
#define __hgiox_h

//フリップ定数
#define GRAPHICS_FLIP_NONE       -1
#define GRAPHICS_FLIP_HORIZONTAL  0
#define GRAPHICS_FLIP_VERTICAL    2

struct BMSCR;

//	for public use

bool hgio_getkey( int kcode );

//	for internal use

int hgio_file_exist( char *fname );
int hgio_file_read( char *fname, void *ptr, int size, int offset );

int hgio_getDesktopWidth( void );
int hgio_getDesktopHeight( void );

#endif
