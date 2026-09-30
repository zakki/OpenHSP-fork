ROOT := ../..
CXX ?= g++
HSPCMP ?= $(ROOT)/hspcmp
CORE := hsp3 hsp3code hsp3debug hsp3int hspvar_core hspvar_double hspvar_int hspvar_label hspvar_str hspvar_struct hspvar_int64 stack strbuf strnote dpmread filepack hsp3crypt hsp3utfcnv hsp3pathio
SOURCES := $(addprefix $(ROOT)/src/hsp3/,$(addsuffix .cpp,$(CORE))) $(ROOT)/src/hsp3/linux/supio_linux.cpp
.PHONY: test known-gaps clean

test: build/runner build/redraw
	python3 run_tests.py --hspcmp $(HSPCMP) --runner build/runner
	./build/redraw

known-gaps: build/runner
	python3 run_tests.py --hspcmp $(HSPCMP) --runner build/runner --known-gaps

build/redraw: redraw.cpp $(ROOT)/src/hsp3dish/hsp3gr_dish.cpp $(wildcard $(ROOT)/src/hsp3dish/*.h)
	mkdir -p build
	$(CXX) -std=c++17 -O0 -g -Wno-write-strings -DHSPEMSCRIPTEN -DHSPDISH -DHSP64 -DHSP_COM_UNSUPPORTED -ffunction-sections -fdata-sections redraw.cpp -Wl,--gc-sections -o $@

build/runner: runner.cpp $(SOURCES) $(wildcard $(ROOT)/src/hsp3/*.h)
	mkdir -p build
	$(CXX) -std=c++17 -O0 -g -Wno-write-strings -DHSPEMSCRIPTEN -DHSP64 -DHSP_COM_UNSUPPORTED -I$(ROOT)/src/hsp3 runner.cpp $(SOURCES) -o $@

clean:
	rm -rf build
