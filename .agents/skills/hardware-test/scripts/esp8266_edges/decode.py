"""decode.py CAPTURE: decode 9600 8N1 bytes from the `edges.hpp` interval dump of GPIO12.

Rebuilds the level timeline from the `<dt><H|L>` pairs and prints the first four bytes of the first three
frames (a frame starts after at least 2 ms idle high).
"""

import pathlib
import re
import sys

BIT_US = 104.17
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
dumps = [m.group(1) for m in re.finditer(r"\[edges(?::\d+)?\]:   (.*)", text)]
t = 0
ev = []
for dt, lv in re.findall(r"(\d+)([HL]) ", "\n".join(d + " " for d in dumps)):
    t += int(dt)
    ev.append((t, lv == "H"))


def level_at(x: float) -> bool:
    lvl = True
    for tt, high in ev:
        if tt > x:
            break
        lvl = high
    return lvl


starts = [
    ev[k][0]
    for k in range(1, len(ev))
    if not ev[k][1] and ev[k][0] - ev[k - 1][0] > 2000
]
for s in starts[:3]:
    out = []
    x = s
    for _ in range(4):
        fe = next((tt for tt, high in ev if not high and tt >= x - BIT_US / 2), None)
        if fe is None:
            break
        bits = [level_at(fe + BIT_US * (k + 1.5)) for k in range(8)]
        stop = level_at(fe + BIT_US * 9.5)
        out.append((fe - s, sum(b << k for k, b in enumerate(bits)), stop))
        x = fe + BIT_US * 10
    print(
        " ".join(f"+{o:5.0f}us {v:02X}{'' if st else '(no stop)'}" for o, v, st in out)
    )
