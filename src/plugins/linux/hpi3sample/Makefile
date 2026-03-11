CC ?= cc
CFLAGS ?= -std=c99 -O2

ROOT := ../../../..
TARGET := hpi3sample.so
SOURCES := main.c hsp3plugin.c

CPPFLAGS += -I. -DHSPLINUX -DHSP64
CFLAGS += -fPIC
LDFLAGS += -shared

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $(SOURCES)

run-cl: $(TARGET)
	$(ROOT)/hspcmp -i -u --compath=$(ROOT)/common/ hpi3sample.as
	$(ROOT)/hsp3cl hpi3sample.ax

run-dish: $(TARGET)
	$(ROOT)/hspcmp -i -u --compath=$(ROOT)/common/ hpi3sample_dish.as
	$(ROOT)/hsp3dish hpi3sample_dish.ax

clean:
	rm -f $(TARGET)
