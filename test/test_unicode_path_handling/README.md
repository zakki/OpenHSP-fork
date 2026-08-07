# Unicode path smoke tests

This suite exercises the compiler path boundary using temporary files whose names contain Japanese characters.

From the repository root, run:

```text
python test/test_unicode_path_handling/run_tests.py --hspcmp src/hspcmp/Release64/hspcmp.exe
```

On Windows, the normal x64 build output can be tested with:

```text
test\test_unicode_path_handling\run_tests.bat
```

The runner currently checks:

- UTF-8 source and UTF-8 `#include` paths
- CP932 source and CP932 `#include` paths
- compiled `.ax` output, included source markers, and the generated source filename marker

The low-level C++ test remains in `test/test_hsp3pathio`. Run it on POSIX with:

```text
make -C test/test_hsp3pathio clean run
```

An already-built pathio executable can also be passed with `--pathio-exe`. Packager/DLL API tests are intentionally separate because the command-line compiler does not expose pack generation directly.
