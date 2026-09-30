// Run the actual HSPEMSCRIPTEN interpreter one instruction at a time on the host.
#include "hsp3.h"
#include "hsp3code.h"
#include "stack.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static int steps, waits, last_tick = -1;
static void fail(const char *message) {
    std::fprintf(stderr, "FAIL: %s (steps=%d)\n", message, steps);
    std::exit(1);
}
static void message(HSPCTX *ctx) {
    if (ctx->runmode == RUNMODE_WAIT || ctx->runmode == RUNMODE_AWAIT) {
        ++waits;
        ctx->runmode = RUNMODE_RUN;
    }
}
static int command(int cmd) {
    HSPCTX *ctx = code_getctx();
    code_next();
    switch (cmd) {
    case 0: { // print, with the same numeric conversion as mes
        std::puts(code_getdsi(""));
        break;
    }
    case 1: { // enqueue two callbacks with distinct snapshots
        unsigned short *label = code_getlb2();
        int restricted = code_geti();
        for (int i = 1; i <= 2; ++i) {
            ctx->iparam = i;
            ctx->wparam = i + 10;
            ctx->lparam = i + 20;
            ctx->stat = i + 30;
            ctx->strsize = i + 40;
            std::strcpy(ctx->refstr, i == 1 ? "first" : "second");
            ctx->refdval = i + 0.5;
            if (restricted) code_callback(label); else code_call(label);
        }
        break;
    }
    case 2: // verify snapshot before HSP expressions overwrite return values
        if (ctx->wparam != ctx->iparam + 10 || ctx->lparam != ctx->iparam + 20 ||
            ctx->stat != ctx->iparam + 30 || ctx->strsize != ctx->iparam + 40 ||
            ctx->refdval != ctx->iparam + 0.5 ||
            std::strcmp(ctx->refstr, ctx->iparam == 1 ? "first" : "second"))
            fail("callback snapshot");
        break;
    case 3:
        if (ctx->callback_flag != code_geti()) fail("callback flag");
        break;
    case 4:
        if (last_tick == steps) fail("command loop did not yield to root executor");
        last_tick = steps;
        break;
    default:
        fail("unknown test command");
    }
    return RUNMODE_RUN;
}
static void check_reset(Hsp3 &hsp) {
    // Reset with one active callback and another still queued. Neither may
    // survive into the next bytecode image, whose label addresses can differ.
    code_callback(code_getpcbak());
    code_callback(code_getpcbak());
    code_emscripten_run_continuation_step();
    if (!code_emscripten_is_continuation_active()) fail("reset setup");
    if (hsp.Reset(0)) fail("reload bytecode");
    if (code_emscripten_is_continuation_active() || code_emscripten_has_continuation() ||
        hsp.hspctx.callback_flag) fail("reset left callback state behind");
}
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    Hsp3 hsp;
    hsp.SetFileName(argv[1]);
    if (hsp.Reset(0)) fail("load bytecode");
    code_gettypeinfo(18)->cmdfunc = command;
    hsp.hspctx.msgfunc = message;
    int prev = 0;
    for (steps = 0; steps < 100000; ++steps) {
        int mode = code_execcmd_one(prev);
        if (mode == RUNMODE_ERROR) {
            std::printf("ERROR %d\n", hsp.hspctx.err);
            return 1;
        }
        if (mode == RUNMODE_END) {
            if (hsp.hspctx.sublev || hsp.hspctx.looplev || StackGetLevel ||
                code_emscripten_is_continuation_active() || hsp.hspctx.callback_flag)
                fail("unbalanced execution state");
            std::printf("DONE waits=%d\n", waits);
            check_reset(hsp);
            return 0;
        }
    }
    fail("instruction budget exceeded");
}
