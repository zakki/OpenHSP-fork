CXX ?= g++
CXXFLAGS ?= -O2 -fPIC -std=c++17 -Wall -Wextra
CPPFLAGS ?= -DHSPLINUX
UNAME_M := $(shell uname -m)

ifeq ($(findstring 64,$(UNAME_M)),64)
CPPFLAGS += -DHSP64
endif

TARGET = hspany.so
SOURCES = hspany.cpp

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -shared -o $@ $<

clean:
	rm -f $(TARGET)

