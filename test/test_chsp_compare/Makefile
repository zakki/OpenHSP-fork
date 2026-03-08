ROOT := ../..
HSPCMP := $(ROOT)/hspcmp
HSP3CL := $(ROOT)/hsp3cl
COMPATH := $(ROOT)/common/
CXX ?= g++
CXXFLAGS ?= -std=c++17 -I$(ROOT)
SOFLAGS ?= -shared -fPIC

HSP_CASES := operators_hsp features_hsp commands_hsp branches_hsp nested_hsp vectors_hsp scalars_hsp trig_hsp random_hsp inline_if_hsp inline_else_hsp inline_else_if_hsp inline_else_block_hsp mixed_if_hsp loop_return_hsp loop_return_value_hsp precedence_hsp separators_hsp continuations_hsp int_literals_hsp array_access_hsp local_arrays_hsp array_return_hsp array_builtin_args_hsp array_control_hsp
CHSP_CASES := operators_chsp features_chsp commands_chsp branches_chsp nested_chsp vectors_chsp scalars_chsp trig_chsp random_chsp inline_if_chsp inline_else_chsp inline_else_if_chsp inline_else_block_chsp mixed_if_chsp loop_return_chsp loop_return_value_chsp precedence_chsp separators_chsp continuations_chsp int_literals_chsp array_access_chsp local_arrays_chsp array_return_chsp array_builtin_args_chsp array_control_chsp
TRANSFORM_STAMPS := $(CHSP_CASES:%=%.transform-ok)

.PHONY: all clean compare transform transform-cli-check

all: compare

%.ax: %.hsp
	$(HSPCMP) -d -i -u --compath=$(COMPATH) $<

%_chsp.ax %_chsp.hsp %_chsp.cpp: %_chsp.chsp
	$(HSPCMP) -d -i -u --compath=$(COMPATH) $<

%_chsp.so: %_chsp.cpp
	$(CXX) $(CXXFLAGS) $(SOFLAGS) $< -o $@

%.out: %.ax
	$(HSP3CL) $< | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

operators_chsp.out: operators_chsp.so operators_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) operators_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

features_chsp.out: features_chsp.so features_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) features_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

commands_chsp.out: commands_chsp.so commands_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) commands_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

branches_chsp.out: branches_chsp.so branches_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) branches_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

nested_chsp.out: nested_chsp.so nested_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) nested_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

vectors_chsp.out: vectors_chsp.so vectors_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) vectors_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

scalars_chsp.out: scalars_chsp.so scalars_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) scalars_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

trig_chsp.out: trig_chsp.so trig_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) trig_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

random_chsp.out: random_chsp.so random_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) random_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

inline_if_chsp.out: inline_if_chsp.so inline_if_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) inline_if_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

inline_else_chsp.out: inline_else_chsp.so inline_else_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) inline_else_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

inline_else_if_chsp.out: inline_else_if_chsp.so inline_else_if_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) inline_else_if_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

inline_else_block_chsp.out: inline_else_block_chsp.so inline_else_block_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) inline_else_block_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

mixed_if_chsp.out: mixed_if_chsp.so mixed_if_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) mixed_if_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

loop_return_chsp.out: loop_return_chsp.so loop_return_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) loop_return_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

loop_return_value_chsp.out: loop_return_value_chsp.so loop_return_value_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) loop_return_value_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

precedence_chsp.out: precedence_chsp.so precedence_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) precedence_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

separators_chsp.out: separators_chsp.so separators_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) separators_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

continuations_chsp.out: continuations_chsp.so continuations_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) continuations_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

int_literals_chsp.out: int_literals_chsp.so int_literals_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) int_literals_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

array_access_chsp.out: array_access_chsp.so array_access_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) array_access_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

local_arrays_chsp.out: local_arrays_chsp.so local_arrays_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) local_arrays_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

array_return_chsp.out: array_return_chsp.so array_return_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) array_return_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

array_builtin_args_chsp.out: array_builtin_args_chsp.so array_builtin_args_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) array_builtin_args_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

array_control_chsp.out: array_control_chsp.so array_control_chsp.ax
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):." $(HSP3CL) array_control_chsp.ax | tr -d '\r' | grep -v '^gpiod initalize failed\.$$' > $@

compare: operators_hsp.out operators_chsp.out features_hsp.out features_chsp.out commands_hsp.out commands_chsp.out branches_hsp.out branches_chsp.out nested_hsp.out nested_chsp.out vectors_hsp.out vectors_chsp.out scalars_hsp.out scalars_chsp.out trig_hsp.out trig_chsp.out random_hsp.out random_chsp.out inline_if_hsp.out inline_if_chsp.out inline_else_hsp.out inline_else_chsp.out inline_else_if_hsp.out inline_else_if_chsp.out inline_else_block_hsp.out inline_else_block_chsp.out mixed_if_hsp.out mixed_if_chsp.out loop_return_hsp.out loop_return_chsp.out loop_return_value_hsp.out loop_return_value_chsp.out precedence_hsp.out precedence_chsp.out separators_hsp.out separators_chsp.out continuations_hsp.out continuations_chsp.out int_literals_hsp.out int_literals_chsp.out array_access_hsp.out array_access_chsp.out local_arrays_hsp.out local_arrays_chsp.out array_return_hsp.out array_return_chsp.out array_builtin_args_hsp.out array_builtin_args_chsp.out array_control_hsp.out array_control_chsp.out
	diff -u operators_hsp.out operators_chsp.out
	diff -u features_hsp.out features_chsp.out
	diff -u commands_hsp.out commands_chsp.out
	diff -u branches_hsp.out branches_chsp.out
	diff -u nested_hsp.out nested_chsp.out
	diff -u vectors_hsp.out vectors_chsp.out
	diff -u scalars_hsp.out scalars_chsp.out
	diff -u trig_hsp.out trig_chsp.out
	diff -u random_hsp.out random_chsp.out
	diff -u inline_if_hsp.out inline_if_chsp.out
	diff -u inline_else_hsp.out inline_else_chsp.out
	diff -u inline_else_if_hsp.out inline_else_if_chsp.out
	diff -u inline_else_block_hsp.out inline_else_block_chsp.out
	diff -u mixed_if_hsp.out mixed_if_chsp.out
	diff -u loop_return_hsp.out loop_return_chsp.out
	diff -u loop_return_value_hsp.out loop_return_value_chsp.out
	diff -u precedence_hsp.out precedence_chsp.out
	diff -u separators_hsp.out separators_chsp.out
	diff -u continuations_hsp.out continuations_chsp.out
	diff -u int_literals_hsp.out int_literals_chsp.out
	diff -u array_access_hsp.out array_access_chsp.out
	diff -u local_arrays_hsp.out local_arrays_chsp.out
	diff -u array_return_hsp.out array_return_chsp.out
	diff -u array_builtin_args_hsp.out array_builtin_args_chsp.out
	diff -u array_control_hsp.out array_control_chsp.out

transform: $(TRANSFORM_STAMPS) transform-cli-check

%.transform-ok: %.chsp $(HSPCMP)
	rm -f $*.ax $*.chi $*.cpp $*.transform.cpp
	$(HSPCMP) -d -i -u -t --compath=$(COMPATH) $<
	test -s $*.chi
	test -s $*.cpp
	test ! -e $*.ax
	grep -q '#module m0' $*.chi
	grep -Eq '#(deffunc|defcfunc|cfunc) ' $*.chi
	cp $*.cpp $*.transform.cpp
	$(HSPCMP) -d -i -u --compath=$(COMPATH) $<
	cmp -s $*.cpp $*.transform.cpp
	touch $@

transform-cli-check: $(HSPCMP)
	rm -f operators_chsp.ax operators_chsp.chi operators_chsp.cpp
	$(HSPCMP) -d -i -u -t --compath=$(COMPATH) operators_chsp.chsp
	test -s operators_chsp.chi
	test -s operators_chsp.cpp
	test ! -e operators_chsp.ax
	rm -f operators_hsp.ax operators_hsp.chi operators_hsp.cpp operators_hsp.i
	$(HSPCMP) -d -i -u -t --compath=$(COMPATH) operators_hsp.hsp
	test -s operators_hsp.chi
	test -s operators_hsp.cpp
	test ! -e operators_hsp.ax
	grep -q 'mes@hsp "OPS"' operators_hsp.chi
	! $(HSPCMP) -p -t --compath=$(COMPATH) operators_chsp.chsp >/dev/null 2>&1

clean:
	rm -f operators_hsp.ax operators_hsp.i operators_hsp.chi operators_hsp.cpp features_hsp.ax features_hsp.i commands_hsp.ax commands_hsp.i branches_hsp.ax branches_hsp.i nested_hsp.ax nested_hsp.i vectors_hsp.ax vectors_hsp.i scalars_hsp.ax scalars_hsp.i trig_hsp.ax trig_hsp.i random_hsp.ax random_hsp.i inline_if_hsp.ax inline_if_hsp.i precedence_hsp.ax precedence_hsp.i
	rm -f operators_chsp.ax operators_chsp.hsp operators_chsp.cpp operators_chsp.i operators_chsp.so
	rm -f features_chsp.ax features_chsp.hsp features_chsp.cpp features_chsp.i features_chsp.so
	rm -f commands_chsp.ax commands_chsp.hsp commands_chsp.cpp commands_chsp.i commands_chsp.so
	rm -f branches_chsp.ax branches_chsp.hsp branches_chsp.cpp branches_chsp.i branches_chsp.so
	rm -f nested_chsp.ax nested_chsp.hsp nested_chsp.cpp nested_chsp.i nested_chsp.so
	rm -f vectors_chsp.ax vectors_chsp.hsp vectors_chsp.cpp vectors_chsp.i vectors_chsp.so
	rm -f scalars_chsp.ax scalars_chsp.hsp scalars_chsp.cpp scalars_chsp.i scalars_chsp.so
	rm -f trig_chsp.ax trig_chsp.hsp trig_chsp.cpp trig_chsp.i trig_chsp.so
	rm -f random_chsp.ax random_chsp.hsp random_chsp.cpp random_chsp.i random_chsp.so
	rm -f inline_if_chsp.ax inline_if_chsp.hsp inline_if_chsp.cpp inline_if_chsp.i inline_if_chsp.so
	rm -f inline_else_hsp.ax inline_else_hsp.i
	rm -f inline_else_chsp.ax inline_else_chsp.hsp inline_else_chsp.cpp inline_else_chsp.i inline_else_chsp.so
	rm -f inline_else_if_hsp.ax inline_else_if_hsp.i
	rm -f inline_else_if_chsp.ax inline_else_if_chsp.hsp inline_else_if_chsp.cpp inline_else_if_chsp.i inline_else_if_chsp.so
	rm -f inline_else_block_hsp.ax inline_else_block_hsp.i
	rm -f inline_else_block_chsp.ax inline_else_block_chsp.hsp inline_else_block_chsp.cpp inline_else_block_chsp.i inline_else_block_chsp.so
	rm -f mixed_if_hsp.ax mixed_if_hsp.i
	rm -f mixed_if_chsp.ax mixed_if_chsp.hsp mixed_if_chsp.cpp mixed_if_chsp.i mixed_if_chsp.so
	rm -f loop_return_hsp.ax loop_return_hsp.i
	rm -f loop_return_chsp.ax loop_return_chsp.hsp loop_return_chsp.cpp loop_return_chsp.i loop_return_chsp.so
	rm -f loop_return_value_hsp.ax loop_return_value_hsp.i
	rm -f loop_return_value_chsp.ax loop_return_value_chsp.hsp loop_return_value_chsp.cpp loop_return_value_chsp.i loop_return_value_chsp.so
	rm -f precedence_chsp.ax precedence_chsp.hsp precedence_chsp.cpp precedence_chsp.i precedence_chsp.so
	rm -f separators_hsp.ax separators_hsp.i
	rm -f separators_chsp.ax separators_chsp.hsp separators_chsp.cpp separators_chsp.i separators_chsp.so
	rm -f continuations_hsp.ax continuations_hsp.i
	rm -f continuations_chsp.ax continuations_chsp.hsp continuations_chsp.cpp continuations_chsp.i continuations_chsp.so
	rm -f int_literals_hsp.ax int_literals_hsp.i
	rm -f int_literals_chsp.ax int_literals_chsp.hsp int_literals_chsp.cpp int_literals_chsp.i int_literals_chsp.so
	rm -f array_access_hsp.ax array_access_hsp.i
	rm -f array_access_chsp.ax array_access_chsp.hsp array_access_chsp.cpp array_access_chsp.i array_access_chsp.so
	rm -f local_arrays_hsp.ax local_arrays_hsp.i
	rm -f local_arrays_chsp.ax local_arrays_chsp.hsp local_arrays_chsp.cpp local_arrays_chsp.i local_arrays_chsp.so
	rm -f array_return_hsp.ax array_return_hsp.i
	rm -f array_return_chsp.ax array_return_chsp.hsp array_return_chsp.cpp array_return_chsp.i array_return_chsp.so
	rm -f array_builtin_args_hsp.ax array_builtin_args_hsp.i
	rm -f array_builtin_args_chsp.ax array_builtin_args_chsp.hsp array_builtin_args_chsp.cpp array_builtin_args_chsp.i array_builtin_args_chsp.so
	rm -f array_control_hsp.ax array_control_hsp.i
	rm -f array_control_chsp.ax array_control_chsp.hsp array_control_chsp.cpp array_control_chsp.i array_control_chsp.so
	rm -f $(CHSP_CASES:%=%.chi) $(CHSP_CASES:%=%.transform.cpp) $(TRANSFORM_STAMPS)
	rm -f operators_hsp.out operators_chsp.out features_hsp.out features_chsp.out commands_hsp.out commands_chsp.out branches_hsp.out branches_chsp.out nested_hsp.out nested_chsp.out vectors_hsp.out vectors_chsp.out scalars_hsp.out scalars_chsp.out trig_hsp.out trig_chsp.out random_hsp.out random_chsp.out inline_if_hsp.out inline_if_chsp.out inline_else_hsp.out inline_else_chsp.out inline_else_if_hsp.out inline_else_if_chsp.out inline_else_block_hsp.out inline_else_block_chsp.out mixed_if_hsp.out mixed_if_chsp.out loop_return_hsp.out loop_return_chsp.out loop_return_value_hsp.out loop_return_value_chsp.out precedence_hsp.out precedence_chsp.out separators_hsp.out separators_chsp.out continuations_hsp.out continuations_chsp.out int_literals_hsp.out int_literals_chsp.out array_access_hsp.out array_access_chsp.out local_arrays_hsp.out local_arrays_chsp.out array_return_hsp.out array_return_chsp.out array_builtin_args_hsp.out array_builtin_args_chsp.out array_control_hsp.out array_control_chsp.out
	rm -f operators_chsp_native features_chsp_native
