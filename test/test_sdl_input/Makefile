CXX ?= g++
FLAGS := -std=c++17 -g -O0 -Wno-write-strings -DHSPDISH -DHSP64 -DHSP_COM_UNSUPPORTED -ffunction-sections -fdata-sections
GP_FLAGS := -DHSPDISHGP -I../../src/hsp3dish/gameplay/src -I../../src/hsp3dish/win32gp -I../../src/hsp3dish/extlib/src -I../../src/hsp3dish/extlib/src/glew
COMMON := ../../src/hsp3dish/sdl_key.cpp ../../src/hsp3dish/hspwnd_dish.cpp ../../src/hsp3dish/geometry.cpp
.PHONY: test
INPUT_HEADERS := ../../src/hsp3dish/sdl_key.h $(wildcard ../../src/hsp3dish/*.h)
build/linux build/emscripten build/linux_gp build/emscripten_gp: $(INPUT_HEADERS)
build/graphics.o: graphics.cpp ../../src/hsp3dish/emscripten/hgiox.cpp
	mkdir -p build
	$(CXX) $(FLAGS) -DHSPLINUX -c $< -o $@
build/linux: runner.cpp build/graphics.o $(COMMON) ../../src/hsp3dish/linux/hsp3dish.cpp
	$(CXX) $(FLAGS) -DHSPLINUX runner.cpp build/graphics.o $(COMMON) -Wl,--gc-sections -lSDL2 -lGL -o $@
build/emscripten: runner.cpp build/graphics.o $(COMMON) ../../src/hsp3dish/emscripten/hsp3dish.cpp
	$(CXX) $(FLAGS) -DHSPEMSCRIPTEN -DTEST_EMSCRIPTEN -Istubs runner.cpp build/graphics.o $(COMMON) -Wl,--gc-sections -lSDL2 -lGL -o $@
build/graphics_gp.o: graphics.cpp ../../src/hsp3dish/win32gp/hgiox.cpp
	mkdir -p build
	$(CXX) $(FLAGS) $(GP_FLAGS) -DHSPLINUX -c $< -o $@
build/linux_gp: runner.cpp build/graphics_gp.o $(COMMON) ../../src/hsp3dish/linux/hsp3dish.cpp
	$(CXX) $(FLAGS) $(GP_FLAGS) -DHSPLINUX runner.cpp build/graphics_gp.o $(COMMON) -Wl,--gc-sections -lSDL2 -lGL -o $@
build/emscripten_gp: runner.cpp build/graphics_gp.o $(COMMON) ../../src/hsp3dish/emscripten/hsp3dish.cpp
	$(CXX) $(FLAGS) $(GP_FLAGS) -DHSPEMSCRIPTEN -DTEST_EMSCRIPTEN -Istubs runner.cpp build/graphics_gp.o $(COMMON) -Wl,--gc-sections -lSDL2 -lGL -o $@
test: build/linux build/emscripten build/linux_gp build/emscripten_gp
	python3 run_tests.py
