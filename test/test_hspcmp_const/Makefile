PREFIX = ..
HSPCMP := $(PREFIX)/../hspcmp
HSP3CL := $(PREFIX)/../hsp3cl
COMPATH := $(PREFIX)/../common/

.SUFFIXES: .hsp .ax

.hsp.ax:
	$(HSPCMP) -d -i -u --compath=$(COMPATH) $<

.PHONY: all run expect diff

all: test_const.ax test_const_mix.ax

run: all
	$(HSP3CL) test_const.ax        > test_const.out
	$(HSP3CL) test_const_mix.ax   > test_const_mix.out

expect: run
	cp test_const.ax test_const.ax.expect
	cp test_const.out test_const.expect
	cp test_const_mix.ax test_const_mix.ax.expect
	cp test_const_mix.out test_const_mix.expect

diff: run
	diff -u test_const.ax.expect test_const.ax
	diff -u test_const.expect test_const.out
	diff -u test_const_mix.ax.expect test_const_mix.ax
	diff -u test_const_mix.expect test_const_mix.out
