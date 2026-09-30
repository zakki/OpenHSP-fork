#pragma once
// Browser entry points are never executed by the host SDL event-loop test.
#define EMSCRIPTEN_KEEPALIVE
#define EM_ASM_(...) ((void)0)
void emscripten_cancel_main_loop();
void emscripten_set_main_loop(...);
