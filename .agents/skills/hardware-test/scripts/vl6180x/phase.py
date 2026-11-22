"""phase.py CAPTURE: publishes (number or NaN) and core warning flag set/clear lines per injection window."""

from collections import Counter
import pathlib
import re
import sys

PHASES = (
    ("no-target injection ON", "1nt"),
    ("no-target injection OFF", "2after_nt"),
    ("read failure injection ON", "1fail"),
    ("read failure injection OFF", "2after_fail"),
    ("overflow injection ON", "3ovf"),
    ("overflow injection OFF", "4after_ovf"),
)
ph, c = "before", Counter()
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    ph = next((p for key, p in PHASES if key in line), ph)
    for n in ("Distance", "Illuminance"):
        if m := re.search(rf"'{n}' >> (\S+)", line):
            c[(ph, n, "nan" if m.group(1) == "nan" else "num")] += 1
    for ev in ("set Warning flag", "cleared Warning flag"):
        if ev in line:
            c[(ph, ev)] += 1
    if m := re.search(r"warn=(\d)", line):
        c[(ph, "regs warn=" + m.group(1))] += 1
for k in sorted(c):
    print(k, c[k])
