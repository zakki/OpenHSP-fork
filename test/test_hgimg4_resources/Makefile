# Invoked from the repository root: make -f test/test_hgimg4_resources/Makefile test-resources
include makefile
RESOURCE_DIR := test/test_hgimg4_resources
RESOURCE_BUILD := $(RESOURCE_DIR)/build
RESOURCE_SOURCES := $(RESOURCE_DIR)/script_bridge.cpp src/hsp3dish/win32gp/gamehsp.cpp src/hsp3dish/win32gp/gpmat.cpp src/hsp3dish/gameplay/src/Ref.cpp src/hsp3dish/gameplay/src/Model.cpp src/hsp3dish/gameplay/src/MaterialParameter.cpp
RESOURCE_OBJS := $(patsubst %.cpp,$(RESOURCE_BUILD)/%.o,$(RESOURCE_SOURCES))
RESOURCE_FLAGS := $(CFLAGS_GP) -g -O1 -fsanitize=address -fno-omit-frame-pointer -fno-pie
.PHONY: test-resources
test-resources: $(RESOURCE_BUILD)/runner
	python3 $(RESOURCE_DIR)/run_tests.py
$(RESOURCE_BUILD)/%.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(RESOURCE_FLAGS) -MMD -MP -c $< -o $@
$(RESOURCE_BUILD)/runner.o: $(RESOURCE_DIR)/runner.cpp
	mkdir -p $(dir $@)
	$(CXX) $(RESOURCE_FLAGS) -MMD -MP -c $< -o $@
$(RESOURCE_BUILD)/runner: $(RESOURCE_BUILD)/runner.o $(RESOURCE_OBJS) $(filter-out src/hsp3dish/emscripten/main.gpo src/hsp3dish/hsp3gr_dish.gpo $(RESOURCE_SOURCES:.cpp=.gpo),$(OBJS_GP)) $(LIBS_GP)
	$(CXX) -fsanitize=address -no-pie $^ -o $@ $(LIBS2) $(LIBS_GP)
-include $(RESOURCE_OBJS:.o=.d) $(RESOURCE_BUILD)/runner.d
