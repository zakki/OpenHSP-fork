#!/usr/bin/env python3
"""Headless HGIMG4 resource tests, with ASan/LSan and real GamePlay objects."""
from pathlib import Path
import argparse
import os
import struct
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
CASES = ['object_first', 'proxy_first', 'texture', 'missing_node', 'non_model',
         'mask', 'mask_size_match', 'mask_size_width_only', 'mask_size_height_only', 'mask_size_neither',
         'load_node', 'load_scene', 'load_material', 'load_bundle',
         'matrix_leak', 'matrix_values', 'matrix_names', 'matrix_survives', 'reset', 'x64']
parser = argparse.ArgumentParser()
parser.add_argument('--runner', type=Path, default=HERE / 'build/runner')
args = parser.parse_args()
runner = args.runner.resolve()
failures = 0
with tempfile.TemporaryDirectory(prefix='hgimg4-resources-') as tmp:
    path = Path(tmp)
    bundle = b'\xabGPB\xbb\r\n\x1a\n' + bytes([1, 2]) + struct.pack('<I', 0)
    for stem in ['empty', 'no_material']:
        (path / (stem + '.gpb')).write_bytes(bundle)
    for stem in ['empty', 'missing']:
        (path / (stem + '.material')).write_text('material test\n{\n}\n')
    # Uncompressed 2x2 RGBA TGA, top-left origin, distinct alpha per pixel.
    header = bytearray(18)
    header[2] = 2
    header[12:16] = struct.pack('<HH', 2, 2)
    header[16:18] = bytes([32, 0x28])
    pixels = b''.join(bytes([30, 20, 10, a]) for a in [0, 64, 128, 255])
    (path / 'mask.tga').write_bytes(header + pixels)
    cases = {name: [name] for name in CASES}
    for source in sorted((HERE / 'scripts').glob('*.hsp')):
        local = path / source.name
        local.write_bytes(source.read_bytes())
        root = HERE.parent.parent
        compiled = subprocess.run([str(root / 'hspcmp'), '-i', '-u', '--compath=' + str(root / 'common') + '/', str(local)],
                                  cwd=tmp, capture_output=True, text=True, timeout=30)
        if compiled.returncode:
            raise RuntimeError(compiled.stdout + compiled.stderr)
        cases['hsp_' + source.stem] = ['script', str(local.with_suffix('.ax'))]
    for name, arguments in cases.items():
        result = subprocess.run([str(runner), *arguments], cwd=tmp,
                                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=1'},
                                capture_output=True, text=True, timeout=20)
        log = result.stdout + result.stderr
        (HERE / 'build' / (name + '.log')).write_text(log)
        ok = result.returncode == 0 and 'PASS' in result.stdout
        print(('PASS ' if ok else 'FAIL ') + name, flush=True)
        if not ok:
            failures += 1
            diagnostics = [line for line in log.splitlines() if any(s in line for s in ['ERROR:', 'Assertion', 'SUMMARY:'])]
            print('\n'.join(diagnostics[:4]) or log[-1000:])
raise SystemExit(bool(failures))
