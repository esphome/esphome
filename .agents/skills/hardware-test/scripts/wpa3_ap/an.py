"""Summarise the PMF AP's join and leave events per station and the BK7238's joins.
Usage: an.py DIR BUILD [AP_SSID]   reads DIR/ap-BUILD.txt and DIR/bk-BUILD.txt (AP_SSID defaults to wd-pmf-ap)
J = join, L<n> = leave with reason n. Reason 3: the station sent a deauth. Reason 2: the AP found a stale
association when the station came back; a run of reason 2 before one join is reconnects the AP rejected.
env DUT_MACS: comma separated station MACs that must appear in the AP events (default: at least two stations).
Exits 1 on a rejected reconnect, an AP that did not switch to WPA2/WPA3, a missing DUT, or a BK7238 that never
joined the AP."""

from itertools import pairwise
import os
from pathlib import Path
import re
import sys

D, b = Path(sys.argv[1]), sys.argv[2]
AP_SSID = sys.argv[3] if len(sys.argv) > 3 else "wd-pmf-ap"
failed = []
ap = (D / f"ap-{b}.txt").read_text(encoding="utf-8", errors="replace").splitlines()
if not any("PMF capable: 0" in line for line in ap) or any(
    "AP CONFIG FAILED" in line for line in ap
):
    failed.append("the AP did not switch to WPA2/WPA3 mixed")
ev = [line for line in ap if "STA JOIN" in line or "STA LEAVE" in line]
mac_re = re.compile(r"([0-9A-F]{2}(?::[0-9A-F]{2}){5})")
macs = sorted({m.group(1) for line in ev if (m := mac_re.search(line))})
rejected = 0
print(f"== {b}")
for m in macs:
    seq = [
        "J" if "JOIN" in line else "L" + re.search(r"reason (\d+)", line).group(1)
        for line in ev
        if m in line
    ]
    # every L2 after the first in a run without a join in between is a rejected reconnect
    rej = sum(1 for prev, cur in pairwise(seq) if prev == cur == "L2")
    rejected += rej
    print(" ", m, " ".join(seq), f"(rejected {rej})")
if rejected:
    failed.append(f"{rejected} rejected reconnects")
if want := [
    x.strip().upper() for x in os.environ.get("DUT_MACS", "").split(",") if x.strip()
]:
    if missing := [x for x in want if x not in macs]:
        failed.append(f"no AP events for {', '.join(missing)}")
elif len(macs) < 2:
    failed.append(f"AP events for {len(macs)} stations, expected both DUTs")
bk = (D / f"bk-{b}.txt").read_text(encoding="utf-8", errors="replace").splitlines()
joins = [
    m.groups() for line in bk if (m := re.search(r"JOINED (.+?) after (\d+) ms", line))
]
print("  bk7238 joins:", " ".join(f"{s}@{int(t) / 1000:.1f}s" for s, t in joins))
print("  bk7238 reboots:", sum("REBOOT on" in line for line in bk))
if not any(s == AP_SSID for s, _ in joins):
    failed.append(f"the BK7238 never joined {AP_SSID}")
print("PASS" if not failed else "FAIL: " + "; ".join(failed))
sys.exit(1 if failed else 0)
