ROOT := ../..
HSPCMP ?= $(ROOT)/hspcmp
HSP3CL ?= $(ROOT)/hsp3cl
COMPATH ?= $(ROOT)/common
C_TARGET_DIR ?= c_target
C_LIBTCC_TARGET_DIR ?= c_target_libtcc
C_NATIVE_CC ?= cc
C_NATIVE_CFLAGS ?= -std=c11 -I$(ROOT)
C_NATIVE_SOFLAGS ?= -shared -fPIC
C_NATIVE_LDLIBS ?= -lm
PYTHON ?= python3
RUNNER := $(PYTHON) ./run_test_matrix.py

export HSPCMP
export HSP3CL
export COMPATH
export C_TARGET_DIR
export C_LIBTCC_TARGET_DIR
export C_NATIVE_CC
export C_NATIVE_CFLAGS
export C_NATIVE_SOFLAGS
export C_NATIVE_LDLIBS

.PHONY: all clean compare compare-c compare-c-libtcc transform transform-c transform-cli-check transform-c-cli-check check-c check-c-tcc check-c-libtcc

all: compare compare-c compare-c-libtcc

compare:
	$(RUNNER) compare --mode default

compare-c:
	$(RUNNER) compare --mode c

compare-c-libtcc:
	$(RUNNER) compare --mode libtcc

transform:
	$(RUNNER) transform --mode default

transform-c:
	$(RUNNER) transform --mode c

transform-cli-check: transform

transform-c-cli-check: transform-c

check-c: compare-c transform-c

check-c-tcc:
	$(MAKE) clean
	$(MAKE) compare-c C_NATIVE_CC=tcc
	$(MAKE) transform-c C_NATIVE_CC=tcc

check-c-libtcc:
	$(MAKE) clean
	$(MAKE) compare-c-libtcc

clean:
	$(RUNNER) clean
