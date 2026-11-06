"""Per disabled window: device connects, inbound messages, broker connects and ticks that reached the broker.

usage: analyze.py OUTDIR NAME   (reads serial.txt, mosq.log and sub.txt written by run.sh)
The broker log has 1 s resolution, so a connect in the second of the enable can show up inside the window.
"""

import pathlib
import re
import sys

D, NAME = pathlib.Path(sys.argv[1]), sys.argv[2]


def t(s: str) -> float:
    h, m, rest = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(rest)


def lines(name: str) -> list[str]:
    if not (D / name).exists():
        return []
    return (D / name).read_text(encoding="utf-8", errors="replace").splitlines()


ser = [
    (t(m.group(1)), m.group(2))
    for line in lines("serial.txt")
    if (m := re.match(r"\[(\d+:\d+:[\d.]+)\](.*)", line))
]
if not ser:
    sys.exit("serial.txt is empty")
wins: list[list] = []
cur = None
cyc = 0
heaps = []
for ts, s in ser:
    if m := re.search(r"CYCLE (\d+) heap (\d+)", s):
        cyc = int(m.group(1))
        heaps.append(int(m.group(2)))
    if "EV DISABLE" in s:
        cur = [ts, None, cyc]
    if "EV ENABLE" in s and cur:
        cur[1] = ts
        wins.append(cur)
        cur = None
if cur:
    cur[1] = ser[-1][0]
    wins.append(cur)
broker = [
    t(m.group(1))
    for line in lines("mosq.log")
    if (m := re.match(r"(\d+:\d+:\d+): New client connected from \S+ as (\S+)", line))
    and m.group(2).startswith(NAME)
]
ticks = [
    t(m.group(1))
    for line in lines("sub.txt")
    if (m := re.match(r"(\d+:\d+:\d+) \S+/tick", line))
]


def within(ts: float, w: list, slack: float = 0.0) -> bool:
    return w[0] + slack <= ts <= w[1]


print(f"{NAME}: {len(wins)} disabled windows, heap at cycles {heaps}")
for w in wins:
    con = sum(1 for ts, s in ser if "EV CONNECT" in s and within(ts, w, 0.5))
    msg = sum(1 for ts, s in ser if "EV MSG" in s and within(ts, w, 1.0))
    bc = sum(1 for ts in broker if within(ts, w, 1.0))
    tk = sum(1 for ts in ticks if within(ts, w, 2.0))
    nxt = next((ts for ts, s in ser if "EV CONNECT" in s and ts > w[1]), None)
    lat = f"{(nxt - w[1]) * 1000:.0f} ms" if nxt else "never"
    print(
        f"  cycle {w[2]}: disabled {w[1] - w[0]:.0f} s  device on_connect={con} inbound msgs={msg} "
        f"broker connects={bc} ticks at broker={tk}  enable->connect {lat}"
    )
