// Use the production HSP command dispatcher without initializing a window/GPU.
#include "../../src/hsp3dish/hsp3gr_dish.cpp"
void setup_resource_script(gamehsp* target) {
    HSP3TYPEINFO* info = code_gettypeinfo(TYPE_EXTCMD);
    ctx = info->hspctx;
    type = info->hspexinfo->nptype;
    val = info->hspexinfo->npval;
    game = target;
    info->cmdfunc = cmdfunc_extcmd;
}
