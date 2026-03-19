#ifndef CHSP_LIBTCC_SHARED_H
#define CHSP_LIBTCC_SHARED_H

#include "../../hsp3/hsp3config.h"
#include "../membuf.h"
#include "chsp_frontend_v2.h"

#if defined( HSPLINUX ) || defined( HSPWIN )
#define CHSP_HAS_LIBTCC 1
#endif

class CHsc3;

#ifdef CHSP_HAS_LIBTCC
int chsp_compile_library_with_libtcc( CHsc3 *hsc3, const std::vector<std::string> &native_files, const char *compath,
									  const std::vector<ChspNativeArtifact> &native_artifacts );
#endif

#endif
