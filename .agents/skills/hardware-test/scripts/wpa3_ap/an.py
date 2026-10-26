"""Summarise the PMF AP's join and leave events per station and the BK7238's joins.
Usage: an.py DIR BUILD   reads DIR/ap-BUILD.txt and DIR/bk-BUILD.txt
J = join, L<n> = leave with reason n. Reason 3: the station sent a deauth. Reason 2: the AP found a stale
association when the station came back; a run of reason 2 before one join is reconnects the AP rejected.
Exits 1 when any station had a rejected reconnect."""

from itertools import pairwise
from pathlib import Path
import re
import sys

D, b = Path(sys.argv[1]), sys.argv[2]
ap = (D / f"ap-{b}.txt").read_text(encoding="utf-8", errors="replace").splitlines()
ev = [line for line in ap if "STA JOIN" in line or "STA LEAVE" in line]
if not ev:
    print("FAIL: no station events in the AP capture")
    sys.exit(1)
mac_re = re.compile(r"([0-9A-F]{2}(?::[0-9A-F]{2}){5})")
rejected = 0
print(f"== {b}")
for m in sorted({mac_re.search(line).group(1) for line in ev}):
    seq = [
        "J" if "JOIN" in line else "L" + re.search(r"reason (\d+)", line).group(1)
        for line in ev
        if m in line
    ]
    # every L2 after the first in a run without a join in between is a rejected reconnect
    rej = sum(1 for prev, cur in pairwise(seq) if prev == cur == "L2")
    rejected += rej
    print(" ", m, " ".join(seq), f"(rejected {rej})")
bk = (D / f"bk-{b}.txt").read_text(encoding="utf-8", errors="replace").splitlines()
joins = [
    re.search(r"JOINED (\S+) after (\d+)", line).groups()
    for line in bk
    if "JOINED" in line
]
print("  bk7238 joins:", " ".join(f"{s[:6]}@{int(t) / 1000:.1f}s" for s, t in joins))
print("  bk7238 reboots:", sum("REBOOT on" in line for line in bk))
print(f"{'PASS' if not rejected else 'FAIL'}: {rejected} rejected reconnects")
sys.exit(1 if rejected else 0)
