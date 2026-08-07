# Unicode Path Test Suite Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a repeatable test runner for Unicode path handling that validates the UTF-8 path layer and the Windows compiler CLI without requiring a test framework.

**Architecture:** Keep the existing C++ `hsp3pathio` unit test as the low-level test and add a Python standard-library integration runner that creates Unicode-named temporary fixtures, invokes a selected `hspcmp` executable, and checks generated files and preprocessor output. Add a Windows batch entry point that discovers the normal x64 build output while allowing an explicit executable path. Packager/DLL tests remain a separate extension point because the CLI does not expose pack generation directly.

**Tech Stack:** C++11, Python 3 standard library, Windows batch, existing `make`-based pathio test.

---

### Task 1: Add integration test fixtures and runner

**Files:**
- Create: `test/test_unicode_path_handling/run_tests.py`
- Create: `test/test_unicode_path_handling/run_tests.bat`
- Create: `test/test_unicode_path_handling/README.md`

**Steps:**

1. Add argument parsing for `--hspcmp`, `--common`, and `--pathio-exe`.
2. Discover `src/hspcmp/Release64/hspcmp.exe` and `src/hspcmp/Release/hspcmp.exe` when `--hspcmp` is omitted; fail with an actionable message if no compiler is available.
3. Create a temporary directory whose path and files contain Japanese characters, while keeping the executable/common paths configurable.
4. Generate a UTF-8 main source and UTF-8 include source, run `hspcmp -d -i -o... --compath=...`, and assert successful `.ax` output plus the include marker and `__file__` source filename.
5. Generate a CP932-encoded main source that includes a Japanese-named file, run without `-i`, and assert successful `.ax` output and the include marker.
6. If `--pathio-exe` is supplied, execute it and propagate failure; document the existing `make -C test/test_hsp3pathio clean run` command for POSIX.
7. Add a batch wrapper using `%~dp0` so it works from any current directory and accepts an optional compiler path.

**Verification:**

Run `python test/test_unicode_path_handling/run_tests.py --hspcmp <hspcmp>` on Windows and expect both compiler cases to pass. Run the existing pathio test separately on POSIX.

**Commit:** `test: add Unicode compiler path smoke runner`

### Task 2: Integrate the runner with the repository test entry point

**Files:**
- Modify: `test/Makefile`
- Modify: `test/test_unicode_path_handling/README.md`

**Steps:**

1. Add a `unicode-path` target that runs the Python runner when `HSPCMP` is provided and prints the required Windows command otherwise.
2. Keep the target non-destructive: all generated fixtures live in a temporary directory and are removed by the runner.
3. Document expected build locations, explicit overrides, and the current scope limitation that DLL pack tests are not yet included.

**Verification:**

Run `make -C test unicode-path HSPCMP=...` on a POSIX-compatible environment with a suitable compiler, and run the batch wrapper on Windows.

**Commit:** `test: expose Unicode path suite from test Makefile`

### Task 3: Verify and hand off the suite

**Files:**
- No source changes unless verification exposes a test defect.

**Steps:**

1. Run the existing C++ pathio test.
2. Run Python syntax compilation for the new runner.
3. Run the runner against an available compiler if present; otherwise verify its argument/discovery error path.
4. Run `git diff --check` and inspect generated-file status.
5. Commit any test-only corrections separately.

**Commit:** Only if required: `test: stabilize Unicode path suite`
