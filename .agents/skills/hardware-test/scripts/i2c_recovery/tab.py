"""tab.py CAPTURE: one line per planted scenario with the recovery result of the boot after it."""

import pathlib
import re
import sys

scen = None
for line in pathlib.Path(sys.argv[1]).read_bytes().decode("latin1").splitlines():
    if m := re.search(r"wedge scenario (\d+): (\w+ after \d bits).*SDA now (\d)", line):
        scen = f"{int(m[1]) % 18:2d} {m[2]} SDA={m[3]}"
    if (m := re.search(r"Recovery: (.*)", line)) and scen:
        print(scen, "=>", m[1])
        scen = None
    if "marked FAILED" in line:
        print("   ", line.split("][")[-1])
