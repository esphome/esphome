"""Leave a partial line behind and then close the stream under the runner.

Run through ``esphome/espidf/runner.py`` by test_espidf_runner.py. Draining
cannot work here; the point is that the failure is reported rather than
raised out of the runner's cleanup, where it would bury the exit code.
"""

import sys

sys.stdout.write("partial before close")
sys.stdout.close()
