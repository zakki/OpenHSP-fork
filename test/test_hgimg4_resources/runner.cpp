// Private access is used only for headless fixture construction and inspection.
// Resource operations use the real gamehsp and GamePlay implementations.
#include "../../src/hsp3dish/gameplay/src/Base.h"
#define private public
#define protected public
#include "../../src/hsp3dish/win32gp/gamehsp.h"
#undef protected
#undef private
#include "../../src/hsp3/hsp3.h"
#include "../../src/hsp3dish/hspwnd.h"
#include "../../src/hsp3dish/hgio.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <sanitizer/lsan_interface.h>

extern gamehsp* game;

// The linked runtime initializes unrelated process-global caches before main.
// Track every allocation made by the fixture and operations under test.
__attribute__((constructor(101))) static void ignore_runtime_startup() { __lsan_disable(); }

struct Fixture {
    gamehsp game;
    Fixture() {
        game._maxobj = game._objpool_max = 4;
        game._objpool_startid = 0;
        game._gpobj = new gpobj[4];
        game._maxmat = 4;
        game._gpmat = new gpmat[4];
        game._curscene = -1;
    }
    ~Fixture() { game.deleteAll(); }
    gpobj* model() {
        Material* material = new Material();
        VertexFormat::Element element(VertexFormat::POSITION, 3);
        Mesh* mesh = new Mesh(VertexFormat(&element, 1)); // no GPU buffer
        Model* model = Model::create(mesh);
        mesh->release();
        model->setMaterial(material);
        material->release();
        gpobj* obj = game.addObj();
        obj->_node = Node::create("root");
        obj->_node->setDrawable(model);
        obj->_model = model;
        model->release();
        return obj;
    }
};
struct OtherDrawable : Drawable {
    unsigned int draw(bool = false) override { return 0; }
    Drawable* clone(NodeCloneContext&) override { return NULL; }
};
static void matrices(gpmat* mat, const char* name, double start) {
    double data[32];
    for (int i = 0; i < 32; ++i) data[i] = start + i;
    assert(mat->setParameter((char*)name, data, 2) == 0);
}
static void check_matrices(Material* material, const char* name, double start) {
    auto* parameter = material->getParameter(name);
    assert(parameter->_count == 2);
    // Matrix's scalar constructor accepts row-major inputs and stores columns.
    for (int n = 0; n < 2; ++n)
        for (int row = 0; row < 4; ++row)
            for (int col = 0; col < 4; ++col)
                assert(parameter->_value.floatPtrValue[n*16 + col*4 + row] == start + n*16 + row*4 + col);
}
static gamehsp* script_game;
extern void setup_resource_script(gamehsp*);
static int script_command(int cmd) {
    code_next();
    if (cmd == 0) {
        assert(code_geti() != 0);
    } else if (cmd == 1) {
        int id = code_geti();
        std::string name = code_gets();
        int start = code_geti();
        auto* mat = script_game->getMat(id);
        Material* material = mat ? mat->_material : script_game->getObj(id)->_model->getMaterial();
        check_matrices(material, name.c_str(), start);
    } else {
        std::abort();
    }
    return RUNMODE_RUN;
}
static void run_script(Fixture& f, const char* path) {
    // Object 0 is a real headless Model. Everything after this setup is HSP.
    f.model();
    // Bytecode loading has its own lifetime, outside the HGIMG4 leak checks.
    __lsan_disable();
    Hsp3 hsp;
    hsp.SetFileName(path);
    assert(hsp.Reset(0) == 0);
    __lsan_enable();
    setup_resource_script(&f.game);
    script_game = &f.game;
    code_gettypeinfo(18)->cmdfunc = script_command;
    hsp.hspctx.msgfunc = [](HSPCTX* context) {
        assert(context->runmode == RUNMODE_END);
    };
    int mode = code_execcmd();
    if (mode != RUNMODE_END) std::fprintf(stderr, "HSP error %d\n", hsp.hspctx.err);
    assert(mode == RUNMODE_END);
}
static void test_x64(Fixture& f) {
    static_assert(sizeof(void*) == 8, "This regression targets x64");
    gpobj* obj = f.game.addObj();
    obj->_spr = new gpspr();
    struct Field { int id; int* pointer; } fields[] = {
        {GPOBJ_PRMSET_MODE, &obj->_mode}, {GPOBJ_PRMSET_ID, &obj->_id},
        {GPOBJ_PRMSET_TIMER, &obj->_timer}, {GPOBJ_PRMSET_MYGROUP, &obj->_mygroup},
        {GPOBJ_PRMSET_COLGROUP, &obj->_colgroup}, {GPOBJ_PRMSET_SHAPE, &obj->_shape},
        {GPOBJ_PRMSET_USEGPMAT, &obj->_usegpmat}, {GPOBJ_PRMSET_USEGPPHY, &obj->_usegpphy},
        {GPOBJ_PRMSET_COLILOG, &obj->_colilog}, {GPOBJ_PRMSET_ALPHA, &obj->_transparent},
        {GPOBJ_PRMSET_FADE, &obj->_fade}, {GPOBJ_PRMSET_SPRID, &obj->_spr->_id},
        {GPOBJ_PRMSET_SPRCELID, &obj->_spr->_celid}, {GPOBJ_PRMSET_SPRGMODE, &obj->_spr->_gmode}
    };
    for (auto field : fields) {
        assert(f.game.getObjectPrmPtr(0, field.id) == field.pointer);
        assert(f.game.setObjectPrm(0, field.id, 0x1234) == 0);
        assert(*field.pointer == 0x1234);
        int value = 0;
        assert(f.game.getObjectPrm(0, field.id, &value) == 0 && value == 0x1234);
        assert(f.game.setObjectPrm(0, field.id, 1, GPOBJ_PRMMETHOD_ON) == 0);
        assert(*field.pointer == 0x1235);
        assert(f.game.setObjectPrm(0, field.id, 4, GPOBJ_PRMMETHOD_OFF) == 0);
        assert(*field.pointer == 0x1231);
    }
    obj->_mark = 0x4321;
    assert(f.game.setObjectPrm(0, GPOBJ_PRMSET_FLAG, 3) == 0);
    int value = 0;
    assert(f.game.getObjectPrm(0, GPOBJ_PRMSET_FLAG, &value) == 0 && value == 3);
    assert(obj->_mark == 0x4321);
    assert(f.game.getObjectPrmPtr(0, 999) == NULL);
    assert(f.game.setObjectPrm(0, 999, 1) == -1);
}
int main(int argc, char** argv) {
    __lsan_enable();
    assert(argc == 2 || argc == 3);
    const std::string name = argv[1];
    Fixture f;
    if (name == "script") { assert(argc == 3); run_script(f, argv[2]); }
    else if (name == "x64") { test_x64(f); }
    else if (name == "mask") {
        int x = 0, y = 0;
        char* mask = f.game.getPixelMaskBuffer((char*)"mask.tga", &x, &y);
        assert(mask && x == 2 && y == 2);
        // GamePlay Image::create stores rows bottom-up.
        const unsigned char expected[] = {128, 255, 0, 64};
        assert(std::memcmp(mask, expected, 4) == 0);
        free(mask);
        assert(f.game.getPixelMaskBuffer((char*)"missing.png", &x, &y) == NULL);
    }
    else if (name.rfind("mask_size_", 0) == 0) {
        BMSCR screen = {};
        screen.sx = name == "mask_size_height_only" || name == "mask_size_neither" ? 3 : 2;
        screen.sy = name == "mask_size_width_only" || name == "mask_size_neither" ? 3 : 2;
        gamehsp* previous = game;
        game = &f.game;
        char* mask = hgio_texmaskbuffer(&screen, (char*)"mask.tga");
        game = previous;
        assert((mask != NULL) == (name == "mask_size_match"));
        free(mask);
    }
    else if (name.rfind("load_", 0) == 0) {
        const char* path = name == "load_bundle" ? "missing" : name == "load_material" ? "no_material" : "empty";
        assert(f.game.makeModelNode((char*)path, name == "load_scene" ? NULL : (char*)"missing", (char*)"") == -1);
        assert(f.game.getObj(0) == NULL);
        // LeakSanitizer verifies Bundle, Material and the temporary root Node.
    }
    else if (name == "matrix_leak") {
        auto* mat = f.game.addMat();
        mat->_material = new Material();
        matrices(mat, "bones", 1);
        f.game.deleteMat(mat->_id);
        assert(mat->_matbuffer == NULL);
    }
    else {
        gpobj* obj = f.model();
        Material* material = obj->_model->getMaterial();
        if (name == "missing_node") {
            assert(f.game.makeNewMatFromObj(obj->_id, 0, (char*)"missing") == -1);
        } else if (name == "non_model") {
            OtherDrawable drawable;
            obj->_node->setDrawable(&drawable);
            assert(f.game.makeNewMatFromObj(obj->_id, 0, (char*)"root") == -1);
            obj->_node->setDrawable(NULL);
        } else {
            int id = f.game.makeNewMatFromObj(obj->_id, 0, (char*)"");
            assert(id >= 0);
            gpmat* mat = f.game.getMat(id);
            if (name == "object_first") {
                f.game.deleteObj(obj->_id);
                assert(mat->setParameter((char*)"alpha", 0.5f) == 0);
                f.game.deleteMat(id);
            } else if (name == "proxy_first") {
                f.game.deleteMat(id);
                material->getParameter("alpha")->setValue(0.5f);
                f.game.deleteObj(obj->_id);
            } else if (name == "texture") {
                material->getParameter("u_diffuseTexture")->setValue(0.5f);
                // Keep an independent owner to test parameter preservation separately.
                material->addRef();
                f.game.deleteMat(id);
                assert(material->getParameterCount() == 1);
                material->release();
            } else if (name == "reset") {
                auto* owned = f.game.addMat();
                owned->_material = new Material();
                f.game.deleteAll(); // LeakSanitizer catches skipped material IDs.
            } else {
                // Keep a separate owner so matrix tests isolate buffer lifetime.
                material->addRef();
                matrices(mat, "bones", 1);
                if (name == "matrix_values") check_matrices(material, "bones", 1);
                if (name == "matrix_names") {
                    matrices(mat, "other", 101);
                    check_matrices(material, "bones", 1);
                    check_matrices(material, "other", 101);
                }
                f.game.deleteMat(id);
                if (name == "matrix_survives") check_matrices(material, "bones", 1);
                material->release();
            }
        }
    }
    puts("PASS");
}
