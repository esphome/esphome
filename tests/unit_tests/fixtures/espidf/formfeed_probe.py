"""Write a form feed part way through the output.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py. A form
feed is not a line terminator here, so everything written must still come
out, including the complete lines that follow it.
"""

import sys

sys.stdout.write("Compiling main.cpp\n")
sys.stdout.write("page one\x0cpage two\n")
sys.stdout.write("[2/9] Building C object\n")
