// Real coordinate conversion and multitouch insertion, without creating a GPU.
// Screen destruction is a GPU sink and is not exercised by these tests.
#define hgio_delscreen unused_hgio_delscreen
#ifdef HSPDISHGP
#include "../../src/hsp3dish/win32gp/hgiox.cpp"
#else
#include "../../src/hsp3dish/emscripten/hgiox.cpp"
#endif
#undef hgio_delscreen
void setup_input_graphics(BMSCR* bm, bool view) {
    mainbm = bm;
    _originX = 160; _originY = 0;
    _rateX = _rateY = 1.0f / 1.5f;
#ifdef HSPDISHGP
    nDestWidth = 640; nDestHeight = 480;
#else
    _bgsx = 640; _bgsy = 480;
#endif
    bm->vp_flag = view ? 1 : 0;
    // Inverse viewport maps normalized coordinates back to pixels, +10 in X.
    mat_unproj = {};
    mat_unproj.m22 = mat_unproj.m33 = 1;
    mat_unproj.m00 = 320;
    mat_unproj.m11 = 240;
    mat_unproj.m30 = 330;
    mat_unproj.m31 = -240;
#ifdef HSPDISHGP
    mat_unproj.m11 = -240;
    mat_unproj.m31 = 240;
#endif
}
