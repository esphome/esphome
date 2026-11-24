"""Heap trend per boot of a scan-repro capture: lowest heap and largest block, and the heap about once a minute.

usage: trend.py CAPTURE
"""

import pathlib
import re
import sys

HEAP_RE = re.compile(
    r"free heap (\d+), min (\d+), max block (\d+), done (\d+), up (\d+)"
)
boots = []
cur = None
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as fh:
    for ln in fh:
        m = HEAP_RE.search(ln)
        if not m:
            continue
        h, mb, up = int(m.group(1)), int(m.group(3)), int(m.group(5))
        if cur is None or up < cur[-1][2]:
            cur = []
            boots.append((ln[1:9], cur))
        cur.append((h, mb, up))
for t, s in boots:
    pts = [
        f"{up}s:{h // 1024}k" for h, mb, up in s if up % 60 < 3 or (h, mb, up) == s[-1]
    ]
    print(
        t,
        f"min {min(h for h, _, _ in s)} minblock {min(mb for _, mb, _ in s)} last up {s[-1][2]}",
        " ".join(pts),
    )
