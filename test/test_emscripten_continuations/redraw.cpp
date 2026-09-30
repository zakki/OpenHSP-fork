// Include the production translation unit to exercise its private phase entry.
// Unused graphics commands are discarded at link time; only drawing sinks are fakes.
#include "../../src/hsp3dish/hsp3gr_dish.cpp"
#include <vector>
#include <string>
#include <iostream>

static std::vector<std::string> events;
Bmscr::Bmscr() { wid = 1; }
Bmscr::~Bmscr() {}
void Bmscr::SendHSPLayerObjectNotice(int layer, int cmd) {
    if (cmd != HSPOBJ_LAYER_CMD_DRAW) std::abort();
    events.push_back("layer:" + std::to_string(layer));
}
int Bmscr::DrawAllObjects() { events.push_back("objects"); return 0; }
void Bmscr::SetDefaultFont() { events.push_back("font"); }
int hgio_redraw(BMSCR *, int flag) {
    events.push_back("redraw:" + std::to_string(flag));
    return 123;
}
void essprite::updateFrame() { events.push_back("sprite"); }
essprite::essprite() {}
essprite::~essprite() {}
ESRectAxis::ESRectAxis() {}

static void check(int flag, std::vector<std::string> expected, int window_id = 1) {
    Bmscr screen;
    screen.wid = window_id;
    essprite sprites;
    sprite = &sprites;
    HSPCTX context = {};
    ctx = &context;
    events.clear();
    hsp3dish_start_redraw_continuation(&screen, flag);
    int steps = 0;
    while (hsp3dish_has_native_continuation()) {
        if (++steps > 4 || hsp3dish_run_native_continuation_step() != RUNMODE_RUN)
            std::abort();
    }
    // An idle step must have no side effects.
    hsp3dish_run_native_continuation_step();
    if (events != expected || context.stat != 123) {
        for (const auto &event : events) std::cerr << event << '\n';
        std::exit(1);
    }
}
int main() {
    for (int flag : {0, 2}) {
        check(flag, {"redraw:" + std::to_string(flag), "font",
              "layer:" + std::to_string(HSPOBJ_OPTION_LAYER_BG),
              "layer:" + std::to_string(HSPOBJ_OPTION_LAYER_NORMAL), "font"});
    }
    for (int flag : {1, 3}) {
        std::vector<std::string> expected = {
              "layer:" + std::to_string(HSPOBJ_OPTION_LAYER_POSTEFF),
              "objects", "font", "layer:" + std::to_string(HSPOBJ_OPTION_LAYER_MAX),
              "redraw:" + std::to_string(flag)};
        check(flag, expected);
        expected.push_back("sprite");
        check(flag, expected, 0);
    }
    std::puts("PASS redraw phases");
}
