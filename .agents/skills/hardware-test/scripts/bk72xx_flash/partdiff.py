"""Compare two full flash dumps of a BK72xx board per partition of its LibreTiny board layout.

usage: partdiff.py BEFORE.bin AFTER.bin LAYOUT.json
LAYOUT.json is a board base file from the LibreTiny platform (for example boards/_base/beken-72xx.json); its
"flash" map ("name": "0xSTART+0xLENGTH") names the partitions. Prints the 4 KB sectors that changed in each one,
plus any changed sector outside the map. Exits 1 when the bootloader, calibration or userdata partition changed.
"""

import json
import pathlib
import sys

before, after = (pathlib.Path(p).read_bytes() for p in sys.argv[1:3])
if len(before) != len(after):
    sys.exit(
        f"dump sizes differ: {len(before)} and {len(after)} bytes; compare two whole chip reads"
    )
parts = []
for name, span in json.loads(pathlib.Path(sys.argv[3]).read_text(encoding="utf-8"))[
    "flash"
].items():
    start, length = (int(x, 16) for x in span.split("+"))
    parts.append((start, start + length, name))
parts.sort()
if (end := max(e for _, e, _ in parts)) > len(before):
    sys.exit(
        f"the layout reaches {end:#x} but the dumps are {len(before):#x} bytes; read the whole chip"
    )
SECTOR = 0x1000
covered = set()
protected = []
for start, end, name in parts:
    changed = [
        a
        for a in range(start, end, SECTOR)
        if before[a : a + SECTOR] != after[a : a + SECTOR]
    ]
    covered.update(range(start, end, SECTOR))
    if changed and name in ("bootloader", "calibration", "userdata"):
        protected.append(name)
    listed = " ".join(hex(a) for a in changed[:4]) if 0 < len(changed) <= 4 else ""
    print(
        f"{name:12} {len(changed):4} of {(end - start) // SECTOR:4} sectors changed {listed}"
    )
other = [
    a
    for a in range(0, len(before), SECTOR)
    if a not in covered and before[a : a + SECTOR] != after[a : a + SECTOR]
]
print(
    f"{'unmapped':12} {len(other):4} sectors changed {' '.join(hex(a) for a in other[:8])}"
)
if protected:
    sys.exit(f"FAIL: {', '.join(protected)} changed")
