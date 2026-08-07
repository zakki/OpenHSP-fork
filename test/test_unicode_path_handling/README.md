# Unicode path smoke tests

This suite exercises the compiler path boundary using temporary files whose names contain Japanese characters.

From the repository root, run:

```text
python test/test_unicode_path_handling/run_tests.py --hspcmp src/hspcmp/Release64/hspcmp.exe
```

To include the Windows DLL and DPM2 pack checks explicitly:

```text
python test/test_unicode_path_handling/run_tests.py --hspcmp src/hspcmp/Release64/hspcmp.exe --hspcmp-dll src/hspcmp/Release64/hspcmp_64.dll
```

On Windows, the normal x64 build output can be tested with:

```text
test\test_unicode_path_handling\run_tests.bat
```

The batch wrapper accepts an optional second argument for the DLL. If omitted,
it discovers `Release64/hspcmp_64.dll` or `Release/hspcmp.dll` when present.

The runner currently checks:

- UTF-8 source and UTF-8 `#include` paths
- CP932 source and CP932 `#include` paths
- compiled `.ax` output, included source markers, and the generated source filename marker
- optional DLL compilation through the legacy CP932 path API
- optional DPM2 pack creation, reload, and extraction with a Japanese asset name

The low-level C++ test remains in `test/test_hsp3pathio`. Run it on POSIX with:

```text
make -C test/test_hsp3pathio clean run
```

An already-built pathio executable can also be passed with `--pathio-exe`. The
DLL/pack extension requires Windows and `--hspcmp-dll`; when omitted, only the
CLI and pathio tests run. The `test` Makefile target accepts the optional
`HSPCMP_DLL` variable as well as `HSPCMP_EXE`.
