"""Print the ESP-IDF version of a given framework root.

Run via ``python <this file> <idf_framework_root>``. PYTHONPATH must include
``<idf_framework_root>/tools`` so ``idf_tools`` is importable.
"""

# pylint: disable=import-error  # idf_tools is on PYTHONPATH at runtime only

import sys

from idf_tools import g, get_idf_version

g.idf_path = sys.argv[1]
print(get_idf_version())
