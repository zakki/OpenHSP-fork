# Usage:
#   make compare
#   make compare MODE=all
#   make compare CASE=pass/nested_gosub_return

PYTHON ?= python3
NATIVE_HSPCMP ?= ../../hspcmp
NATIVE_HSP3CL ?= ../../hsp3cl_coretest
NODE ?= node
HSP3CL_EM ?= ../../hsp3cl_loop_test.js
MODE ?= pass

.PHONY: compare

compare:
	$(PYTHON) run_compare.py \
		--mode $(MODE) \
		--native-hspcmp $(NATIVE_HSPCMP) \
		--native-hsp3cl $(NATIVE_HSP3CL) \
		--emscripten-hsp3cl $(HSP3CL_EM) \
		--node $(NODE) \
		$(if $(CASE),cases/$(CASE).hsp,)
