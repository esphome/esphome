"""plat.py CAPTURE [--exact]: frame counter deltas between idle periods (two or more probes with no new frames) and
the heap there. --exact prints the final play plus 20 plays frame count of a run.py flow instead: from the point where
the last announcement ends and both counters hold still for two probes to the end, so two builds compare exactly."""

import pathlib
import re
import sys

KEYS = ("med", "ann", "int_free", "int_largest", "ps_free", "ps_largest")
rows = []
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    if "[probe" in line:
        vals = [int(re.search(k + r" (\d+)", line).group(1)) for k in KEYS]
        rows.append((line[1:13], *vals))
if not rows:
    sys.exit("no probe lines")
if "--exact" in sys.argv[2:]:
    ma = [r[1:3] for r in rows]
    i = max(i for i in range(1, len(ma)) if ma[i][1] != ma[i - 1][1])
    while not (ma[i] == ma[i + 1] == ma[i + 2]):
        i += 1
    print(
        sys.argv[1],
        "final play + 20 plays:",
        ma[-1][0] - ma[i][0],
        "announcement frames:",
        ma[-1][1],
        "med total:",
        ma[-1][0],
    )
    sys.exit()
plats, i = [], 0
while i < len(rows):
    j = i
    while j + 1 < len(rows) and rows[j + 1][1:3] == rows[i][1:3]:
        j += 1
    if j > i:
        plats.append(rows[j])  # last probe of the idle run
    i = j + 1
prev = None
for p in plats:
    d = "" if prev is None else f"med+{p[1] - prev[1]:>7} ann+{p[2] - prev[2]:>6}"
    print(
        f"{p[0]} {d:28} int_free {p[3]} int_largest {p[4]} ps_free {p[5]} ps_largest {p[6]}"
    )
    prev = p
print("total med", rows[-1][1], "ann", rows[-1][2])
