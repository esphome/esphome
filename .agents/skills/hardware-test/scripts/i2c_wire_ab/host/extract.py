"""extract.py TREE OUTDIR: copy the write_readv/transfer_ definitions of the Arduino and ESP-IDF buses out of TREE."""

from pathlib import Path
import re
import shutil
import sys

tree, out = Path(sys.argv[1]), Path(sys.argv[2])
for cls, src, dst in (
    ("ArduinoI2CBus", "i2c_bus_arduino.cpp", "wab_gen_arduino.inc"),
    ("IDFI2CBus", "i2c_bus_esp_idf.cpp", "wab_gen_idf.inc"),
):
    lines = (tree / "esphome/components/i2c" / src).read_text().splitlines()
    pat = re.compile(rf"^ErrorCode {cls}::(write_readv|transfer_)\(")
    starts = [i for i, line in enumerate(lines) if pat.match(line)]
    assert len(starts) == 1, (src, starts)
    s = starts[0]
    e = next(i for i in range(s, len(lines)) if lines[i] == "}")
    (out / dst).write_text("\n".join(lines[s : e + 1]) + "\n")
    print(dst, lines[s].split("(")[0], e - s + 1, "lines")

shutil.copy(
    tree / "esphome/components/i2c/i2c_bus_software.cpp", out / "wab_sw_impl.inc"
)
shutil.copy(
    tree / "esphome/components/i2c/i2c_bus_software.h", out / "i2c_bus_software.h"
)
h = out / "i2c_bus_software.h"
h.write_text(
    h.read_text().replace(
        '#include "i2c_bus.h"', '#include "esphome/components/i2c/i2c_bus.h"'
    )
)
