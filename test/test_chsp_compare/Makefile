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
TEMPLATE_GENERATOR := $(PYTHON) ./generate_templates.py

export HSPCMP
export HSP3CL
export COMPATH
export C_TARGET_DIR
export C_LIBTCC_TARGET_DIR
export C_NATIVE_CC
export C_NATIVE_CFLAGS
export C_NATIVE_SOFLAGS
export C_NATIVE_LDLIBS

.PHONY: all clean generate-templates clean-templates compare compare-emit-c compare-libtcc check-emit-c check-emit-c-tcc check-libtcc

all: compare compare-emit-c compare-libtcc

generate-templates:
	$(TEMPLATE_GENERATOR)

clean-templates:
	$(TEMPLATE_GENERATOR) --clean

compare:
	$(RUNNER) compare --mode default

compare-emit-c:
	$(RUNNER) compare --mode emit-c

compare-libtcc:
	$(RUNNER) compare --mode libtcc

check-emit-c: compare-emit-c

check-emit-c-tcc:
	$(MAKE) clean
	$(MAKE) compare-emit-c C_NATIVE_CC=tcc

check-libtcc:
	$(MAKE) clean
	$(MAKE) compare-libtcc

clean:
	$(TEMPLATE_GENERATOR) --clean
	$(RUNNER) clean
