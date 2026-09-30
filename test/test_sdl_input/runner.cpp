// Exercise the real SDL event loop; only interpreter/GUI sinks are replaced.
#ifdef TEST_EMSCRIPTEN
#include "../../src/hsp3dish/emscripten/hsp3dish.cpp"
#else
#include "../../src/hsp3dish/linux/hsp3dish.cpp"
#endif
#include "../../src/hsp3dish/texmes.h"
#include <cassert>
#include <vector>
#include <array>
#include <cstdio>
#include <cstring>

static Bmscr screen;
static std::vector<std::array<int, 4>> interrupts;
static bool key_irq = true;
static int object_notices = 0;
static void* get_screen(int) { return (BMSCR*)&screen; }
int code_isirq(int id) { return id == HSPIRQ_ONKEY && key_irq; }
int code_sendirq(int id, int i, HSPPTRINT w, HSPPTRINT l) {
    interrupts.push_back({id, i, (int)w, (int)l});
    return RUNMODE_RUN;
}
int code_execcmd2() { return RUNMODE_RUN; }
void code_puterror(HSPERROR) { std::abort(); }
int code_catcherror(HSPERROR) { std::abort(); }
void Bmscr::SendHSPObjectNotice(int) { ++object_notices; }
int Bmscr::UpdateAllObjects() { return 0; }
void Bmscr::ResetHSPObject() {}
void hgio_delscreen(BMSCR*) {}
texmesManager::texmesManager() {}
texmesManager::~texmesManager() {}
extern void setup_input_graphics(BMSCR*, bool);
static void dispatch(SDL_Event event) {
    assert(SDL_PushEvent(&event) == 1);
    assert(handleEvent() == 0);
}
static void key(SDL_Scancode scan, SDL_Keycode sym, bool repeat = false) {
    SDL_Event e = {};
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = scan;
    e.key.keysym.sym = sym;
    e.key.repeat = repeat;
    dispatch(e);
}
static void touch(Uint32 type, SDL_FingerID id, float x, float y) {
    SDL_Event e = {};
    e.type = type;
    e.tfinger.windowID = SDL_GetWindowID(window);
    e.tfinger.fingerId = id;
    e.tfinger.x = x;
    e.tfinger.y = y;
    dispatch(e);
}
int main(int argc, char** argv) {
    assert(argc == 2);
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0);
    window = SDL_CreateWindow("test", 0, 0, 1280, 960, SDL_WINDOW_HIDDEN);
    assert(window);
    screen.sx = 640; screen.sy = 480; screen.vp_flag = 0;
    screen.resetMTouch();
    HSPEXINFO info = {};
    info.HspFunc_getbmscr = get_screen;
    exinfo = &info;
#ifdef TEST_EMSCRIPTEN
    hsp_sx = 1280; hsp_sy = 960;
#endif
    setup_input_graphics((BMSCR*)&screen, std::strcmp(argv[1], "view") == 0);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    if (std::strcmp(argv[1], "keys") == 0) {
        key(SDL_SCANCODE_A, SDLK_a);
        key(SDL_SCANCODE_RETURN, SDLK_RETURN);
        key(SDL_SCANCODE_LEFT, SDLK_LEFT);
        key(SDL_SCANCODE_F1, SDLK_F1);
        key(SDL_SCANCODE_A, SDLK_a, true);
        key(SDL_SCANCODE_KP_1, SDLK_KP_1);
        key(SDL_SCANCODE_KP_MULTIPLY, SDLK_KP_MULTIPLY);
        key(SDL_SCANCODE_KP_PLUS, SDLK_KP_PLUS);
        key(SDL_SCANCODE_KP_MINUS, SDLK_KP_MINUS);
        key(SDL_SCANCODE_KP_PERIOD, SDLK_KP_PERIOD);
        key(SDL_SCANCODE_KP_DIVIDE, SDLK_KP_DIVIDE);
        key(SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER);
        key(SDL_SCANCODE_LSHIFT, SDLK_LSHIFT);
        assert(interrupts.size() == 13);
        const int chars[] = {'A', 13, 0, 0, 'A', '1', '*', '+', '-', '.', '/', 13, 0};
        const int virtual_keys[] = {65, 13, 37, 112, 65, 97, 106, 107, 109, 110, 111, 13, 16};
        for (int i = 0; i < 13; ++i) {
            assert(interrupts[i][0] == HSPIRQ_ONKEY);
            assert(interrupts[i][1] == chars[i]);
            assert(interrupts[i][2] == virtual_keys[i]);
            assert((interrupts[i][3] & 0xffff) == 1);
        }
        assert((interrupts[4][3] & (1 << 30)) != 0);
        assert(get_key_state(SDL_SCANCODE_A));
        SDL_Event up = {};
        up.type = SDL_KEYUP;
        up.key.keysym.scancode = SDL_SCANCODE_A;
        dispatch(up);
        assert(!get_key_state(SDL_SCANCODE_A));
        assert(interrupts.size() == 13);
        assert(object_notices >= 3);
        key_irq = false;
        key(SDL_SCANCODE_B, SDLK_b);
        assert(interrupts.size() == 13);
    } else {
        touch(SDL_FINGERDOWN, 11, 0.5f, 0.5f);
        HSP3MTOUCH* first = screen.getMTouchByPointId(11);
        assert(first);
        // Fixture has a 160px left margin and a scale of 1.5 on both axes.
        int dx = std::strcmp(argv[1], "view") == 0 ? 10 : 0;
        assert(first->x == 320 + dx && first->y == 320);
        touch(SDL_FINGERDOWN, 22, 0.6875f, 0.25f);
        HSP3MTOUCH* second = screen.getMTouchByPointId(22);
        assert(second && second != first);
        assert(second->x == 480 + dx && second->y == 160);
        touch(SDL_FINGERMOTION, 11, 0.3125f, 0.5f);
        assert(first->x == 160 + dx && first->y == 320);
        touch(SDL_FINGERUP, 11, 0.3125f, 0.5f);
        assert(!screen.getMTouchByPointId(11));
        assert(screen.getMTouchByPointId(22));
        touch(SDL_FINGERUP, 22, 0.6875f, 0.25f);
        assert(!screen.getMTouchByPointId(22));
    }
    SDL_DestroyWindow(window); window = NULL;
    SDL_Quit();
    puts("PASS");
}
