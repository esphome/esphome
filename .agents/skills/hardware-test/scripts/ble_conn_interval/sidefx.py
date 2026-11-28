"""sidefx.py TAG [SCENARIO...]: coexistence side effects for one build: ping RTT inside uncached discovery windows
(Connecting .. Discovery finished or disconnect, from cap-TAG.txt) against outside them, TCP buffer full and API close
counts, and the forwarded advert rate the driver saw during attempts."""

import json
import pathlib
import re
import statistics
import sys
import time

from devtime import ts
from stats import quantile

tag = sys.argv[1]
lines = (
    pathlib.Path(f"cap-{tag}.txt")
    .read_text(encoding="utf-8", errors="replace")
    .splitlines()
)
pings = [
    ln.split()
    for ln in pathlib.Path(f"ping-{tag}.txt").read_text(encoding="utf-8").splitlines()
]


# map local seconds of day to epoch using the date of the first ping sample
base = time.mktime(time.localtime(float(pings[0][0]))[:3] + (0, 0, 0, 0, 0, -1))
win: list[tuple[float, float]] = []
cur: dict[str, float] = {}
tcp: list[float] = []
closes = 0
for line in lines:
    t = ts(line)
    if t is None:
        continue
    t += base
    if m := re.search(r"\]: \[(\d)\] 0x\w+ Connecting", line):
        cur[m[1]] = t
    elif (
        m := re.search(
            r"\]: \[(\d)\] \[[^]]*\] (Discovery finished|Disconnected)", line
        )
    ) and m[1] in cur:
        win.append((cur.pop(m[1]), t))
    if "TCP buffer full" in line:
        tcp.append(t)
    if "CONNECTION_CLOSED" in line or "Reading failed" in line:
        closes += 1


def inwin(t: float, pad: float = 0) -> bool:
    return any(a - pad <= t <= b + pad for a, b in win)


def p95(v: list[float]) -> float:
    return quantile(v, 0.95) if v else float("nan")


ins: list[float] = []
outs: list[float] = []
lin = lout = 0
for p in pings:
    t = float(p[0])
    if not win or t < win[0][0] - 60 or t > win[-1][1] + 60:
        continue
    if inwin(t):
        if p[1] == "lost":
            lin += 1
        else:
            ins.append(float(p[1]))
    elif not inwin(t, 3):
        if p[1] == "lost":
            lout += 1
        else:
            outs.append(float(p[1]))
print(f"{tag}: windows={len(win)} ({sum(b - a for a, b in win):.0f} s)")
print(
    f"  ping in windows: n={len(ins)} med={statistics.median(ins) if ins else 0:.0f} p95={p95(ins):.0f} ms lost={lin}"
)
print(
    f"  ping outside   : n={len(outs)} med={statistics.median(outs) if outs else 0:.0f} p95={p95(outs):.0f} ms lost={lout}"
)
print(
    f"  TCP buffer full: {len(tcp)} total, {sum(inwin(t) for t in tcp)} inside windows; API closes {closes}"
)
scs = sys.argv[2:] or [
    p.name[len(tag) + 1 : -6] for p in pathlib.Path().glob(f"{tag}-*.jsonl")
]
for sc in scs:
    detail = [
        a
        for ln in pathlib.Path(f"{tag}-{sc}.jsonl")
        .read_text(encoding="utf-8")
        .splitlines()
        for a in json.loads(ln).get("detail", [])
    ]
    ar = [a["adv_per_s"] for a in detail if a.get("adv_per_s") is not None]
    sn = [a.get("scanner", "") for a in detail if "scanner" in a]
    if ar:
        print(
            f"  {sc}: adverts/s during attempts med={statistics.median(ar)} min={min(ar)} n={len(ar)}; "
            f"scanner not RUNNING {sum('RUNNING' not in s for s in sn)}/{len(sn)}"
        )
