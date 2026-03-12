CC ?= cc
CFLAGS ?= -std=c99 -O2

ROOT := ../../..
TARGET := hpi3sample.so
SOURCES := main.c hsp3plugin.c

CPPFLAGS += -I$(ROOT)/src/hsp3 -DHSPLINUX -DHSP64
CFLAGS += -fPIC
LDFLAGS += -shared

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)
