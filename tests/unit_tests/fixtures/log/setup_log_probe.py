"""Report whether setup_log() pulled in colorama, then print a colored line.

Executed as a subprocess by test_log.py because module imports are
process-global: the parent prints ``colorama_loaded=True/False`` plus an
ANSI colored line so the caller can observe whether the codes survive to
the stream. Pass ``--dashboard`` to simulate a dashboard-spawned run.
"""

import sys

from esphome.core import CORE
from esphome.log import setup_log

if "--dashboard" in sys.argv:
    CORE.dashboard = True

setup_log()

print(f"colorama_loaded={'colorama' in sys.modules}")
print("\033[31mred\033[0m end")
sys.stdout.flush()
