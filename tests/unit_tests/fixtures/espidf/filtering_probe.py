"""Write a mix of noisy and useful build lines, without flushing.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py. The
runner's shim owns both the filtering and the flushing, so this script
only writes.
"""

import sys

sys.stdout.write("Project build complete.\n")
sys.stdout.write("Compiling main.cpp\n")
sys.stdout.write("-- Component paths: /a /b /c\n")
sys.stdout.write("[2/9] Building C object\n")
# No terminator, so the shim has to hold this one back.
sys.stdout.write("still going")
