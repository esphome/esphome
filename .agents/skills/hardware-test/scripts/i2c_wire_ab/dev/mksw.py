"""mksw.py TREE OUT: the tree's software bus (header + source) as one header for an ESP32 build."""

from pathlib import Path
import sys

t = Path(sys.argv[1]) / "esphome/components/i2c"
h = (
    (t / "i2c_bus_software.h")
    .read_text()
    .replace("#pragma once", "")
    .replace('#include "i2c_bus.h"', '#include "esphome/components/i2c/i2c_bus.h"')
)
c = (
    (t / "i2c_bus_software.cpp")
    .read_text()
    .replace('#include "i2c_bus_software.h"', "")
)
Path(sys.argv[2]).write_text(
    "#pragma once\n#define USE_LIBRETINY\n" + h + "\n" + c + "\n#undef USE_LIBRETINY\n",
    encoding="utf-8",
)
