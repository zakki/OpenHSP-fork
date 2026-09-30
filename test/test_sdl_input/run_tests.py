#!/usr/bin/env python3
"""Inject real SDL events into the Linux and Emscripten event handlers."""
from pathlib import Path
import os
import subprocess
HERE = Path(__file__).resolve().parent
failures = 0
for backend in ['linux', 'emscripten', 'linux_gp', 'emscripten_gp']:
    for case in ['keys', 'touch', 'view']:
        result = subprocess.run([str(HERE / 'build' / backend), case],
                                env={**os.environ, 'SDL_VIDEODRIVER': 'dummy'},
                                capture_output=True, text=True, timeout=10)
        ok = result.returncode == 0
        print(('PASS ' if ok else 'FAIL ') + backend + '/' + case, flush=True)
        if not ok:
            failures += 1
            print(result.stdout + result.stderr)
raise SystemExit(bool(failures))
