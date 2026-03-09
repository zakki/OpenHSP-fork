#ifndef CHSP_LIBTCC_SHARED_H
#define CHSP_LIBTCC_SHARED_H

#include "../hsp3/hsp3config.h"
#include "membuf.h"

#if defined(HSPLINUX) || defined(HSPWIN)
#define CHSP_HAS_LIBTCC 1
#endif

class CHsc3;

#ifdef CHSP_HAS_LIBTCC
int chsp_compile_library_with_libtcc( CHsc3 *hsc3, const char *native_file, const char *compath, CMemBuf &hsp_out );
#endif

#endif
