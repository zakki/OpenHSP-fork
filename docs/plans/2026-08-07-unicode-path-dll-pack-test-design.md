# Unicode path DLL and pack test design

## Goal

Extend `test/test_unicode_path_handling` beyond the command-line compiler so
that the Windows `hspcmp` DLL's legacy CP932 path boundary and DPM2 pack path
handling are exercised with the same temporary Japanese-named fixtures.

## Scope and contract

The DLL test uses the existing public exports through Python `ctypes`:

- `hsc_ini`, `hsc_compath`, `hsc_objname`, and `hsc_comp` compile a CP932 source
  with a Japanese-named include into a Japanese-named `.ax` file.
- `hsc_comp` is called with the pack-generation option so it writes the
  compiler-generated `packfile` in the temporary working directory.
- `pack_ini`, `pack_make`, `pack_view`, and `pack_get` create a DPM2 file,
  reload it, and extract a Japanese-named asset.

The DLL's existing API accepts CP932 `char*` paths. The test therefore encodes
path arguments with Windows' `mbcs` codec and verifies the resulting files via
Python's Unicode filesystem API. It does not change the API contract to UTF-8.

The DLL test is optional because POSIX environments do not provide the
Windows DLL. An explicitly supplied missing DLL is an error; omission skips
only this extension while retaining the CLI and pathio tests.

## Runner interface

Add `--hspcmp-dll PATH` to the Python runner. The Windows batch wrapper accepts
an optional second argument and otherwise discovers `Release64/hspcmp_64.dll`
or `Release/hspcmp.dll` beside the CLI build. The Makefile target accepts
`HSPCMP_DLL` as an optional override.

All fixtures and generated `packfile`/`.dpm` files live under the runner's
temporary directory. The runner changes the process working directory only
around DLL calls and restores it in a `finally` block.

## Error handling and verification

Each DLL call checks its integer return value. On failure, the runner calls
`hsc_getmes` when available and includes the DLL message in the exception.
Pack tests additionally require the generated DPM file, extracted asset, and
matching asset bytes. Existing CLI and pathio tests remain unchanged.
