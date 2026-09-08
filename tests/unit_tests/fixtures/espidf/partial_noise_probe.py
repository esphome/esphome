"""End on an unterminated line that the filter is supposed to drop.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py, to
check that releasing a held-back line still applies the filter.
"""

import sys

sys.stdout.write("Compiling main.cpp\n")
sys.stdout.write("Project build complete.")
