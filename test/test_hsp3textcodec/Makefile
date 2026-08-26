PREFIX ?= ../..
CXX ?= g++
CPPFLAGS += -I$(PREFIX)/src/hsp3 $(shell pkg-config --cflags icu-uc)
CXXFLAGS ?= -std=c++17 -Wall -Wextra
LDLIBS += $(shell pkg-config --libs icu-uc)

.PHONY: all clean run

all: test_hsp3textcodec

test_hsp3textcodec: test_hsp3textcodec.cpp $(PREFIX)/src/hsp3/hsp3textcodec.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDLIBS) -o $@

run: test_hsp3textcodec
	./test_hsp3textcodec

clean:
	rm -f test_hsp3textcodec
