"""Prefix each stdin line with a wall clock timestamp (ms)."""

import sys
import time

for line in sys.stdin.buffer:
    sys.stdout.write(f"{time.time():.3f} {line.decode(errors='replace')}")
    sys.stdout.flush()
