"""Per disabled window: device connects, inbound messages, broker connects and ticks that reached the broker.

usage: analyze.py OUTDIR NAME   (reads serial.txt, mosq.log and sub.txt written by run.sh)
The broker log has 1 s resolution, so a connect in the second of the enable can show up inside the window.
Exits 1 when no window was found or a window has a device on_connect or an inbound message.
"""

import bisect
import pathlib
import re
import sys

D, NAME = pathlib.Path(sys.argv[1]), sys.argv[2]


DAY = 86400.0


def t(s: str) -> float:
    h, m, rest = s.split(":")
    return int(h) * 3600 + int(m) * 60 + float(rest)


def unwrap(times: list[float], ref: float) -> list[float]:
    """Add whole days so the clock times run on past midnight, starting from the day nearest REF."""
    out: list[float] = []
    prev = ref
    for x in times:
        x += round((prev - x) / DAY) * DAY
        out.append(x)
        prev = x
    return out


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
REF = ser[0][0]
ser = list(zip(unwrap([ts for ts, _ in ser], REF), [s for _, s in ser], strict=True))
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
broker = unwrap(
    [
        t(m.group(1))
        for line in lines("mosq.log")
        if (
            m := re.match(
                r"(\d+:\d+:\d+): New client connected from \S+ as (\S+)", line
            )
        )
        and m.group(2).startswith(NAME)
    ],
    REF,
)
ticks = unwrap(
    [
        t(m.group(1))
        for line in lines("sub.txt")
        if (m := re.match(r"(\d+:\d+:\d+) \S+/tick", line))
    ],
    REF,
)


connects = [ts for ts, s in ser if "EV CONNECT" in s]
msgs = [ts for ts, s in ser if "EV MSG" in s]


def within(times: list[float], w: list, slack: float) -> int:
    """How many of the sorted TIMES fall in [start + SLACK, end] of window W."""
    return max(
        0, bisect.bisect_right(times, w[1]) - bisect.bisect_left(times, w[0] + slack)
    )


print(f"{NAME}: {len(wins)} disabled windows, heap at cycles {heaps}")
bad = 0
for w in wins:
    con = within(connects, w, 0.5)
    msg = within(msgs, w, 1.0)
    bc = within(broker, w, 1.0)
    tk = within(ticks, w, 2.0)
    i = bisect.bisect_right(connects, w[1])
    nxt = connects[i] if i < len(connects) else None
    lat = f"{(nxt - w[1]) * 1000:.0f} ms" if nxt else "never"
    print(
        f"  cycle {w[2]}: disabled {w[1] - w[0]:.0f} s  device on_connect={con} inbound msgs={msg} "
        f"broker connects={bc} ticks at broker={tk}  enable->connect {lat}"
    )
    bad += con > 0 or msg > 0
if not wins or bad:
    sys.exit(
        f"{bad} of {len(wins)} disabled windows had a device connect or an inbound message"
    )
