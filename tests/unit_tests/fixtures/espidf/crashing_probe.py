"""Die part way through a line, the way a build that blows up does.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py. The
message has no trailing newline, so the runner's shim is holding it when
the process exits; nothing else will ever come to release it.
"""

import sys

sys.stdout.write("FATAL: ld returned 1 exit status")
sys.exit(2)
