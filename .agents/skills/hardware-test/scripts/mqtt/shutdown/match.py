"""match.py OUTDIR: for each event, say whether a bye/dead for that board arrived within 5 s."""

import pathlib
import sys

d = pathlib.Path(sys.argv[1])


def secs(stamp: str) -> int:
    h, m, s = (int(x) for x in stamp.split(":"))
    return h * 3600 + m * 60 + s


ev = [ln.split() for ln in (d / "events.txt").read_text(encoding="utf-8").splitlines()]
br = [
    f
    for f in (
        ln.split() for ln in (d / "broker.txt").read_text(encoding="utf-8").splitlines()
    )
    if len(f) >= 3 and f[2] in ("bye", "dead")
]
rows = {}
for ts, kind, b in ev:
    hit = [
        p
        for s, top, p in br
        if top.startswith(f"sd{b}") and 0 <= secs(s) - secs(ts) <= 5
    ]
    rows.setdefault((b, kind), []).append(
        "bye" if "bye" in hit else (hit[0] if hit else "-")
    )
for (b, kind), r in sorted(rows.items()):
    print(f"{b} {kind:8s} {r.count('bye')}/{len(r)} bye   {' '.join(r)}")
