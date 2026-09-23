"""Print one line, then stay alive so the caller can prove it streamed.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py. The
runner wraps stdout in its filtering shim, so this script deliberately
does not flush: the shim has to do it. The long sleep keeps the process
running, so anything the caller reads must have arrived while the build
was still going rather than at exit.
"""

import sys
import time

sys.stdout.write("Compiling main.cpp\n")
time.sleep(60)
