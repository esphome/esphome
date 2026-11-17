"""maj.py NAME_BASE NAME_PR: per case majority of rc+wire hash over NAME-ser.txt, -ser2, -ser3 (reads: verdict), then
base vs PR."""

from collections import Counter
import pathlib
import re
import sys


def load(p):
    d = {}
    with pathlib.Path(p).open(encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.search(
                r"SN (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|.*\|fnv=([0-9A-F]+)\|(OK|DIFF)",
                line,
            )
            if m:
                k = m.group(1)
                isread = k.startswith(("rreg", "read", "write1+read"))
                d[k] = (
                    m.group(2),
                    m.group(5) if isread else m.group(4),
                    int(m.group(3)),
                )
    return d


def majority(name):
    runs = [load(f"{name}-ser{s}.txt") for s in ("", "2", "3")]
    out, unstable = {}, 0
    for k in runs[0]:
        c = Counter((r[k][0], r[k][1]) for r in runs if k in r)
        v, n = c.most_common(1)[0]
        unstable += len(c) > 1
        out[k] = (v, n, max(r[k][2] for r in runs if k in r))
    return out, unstable


b, ub = majority(sys.argv[1])
p, up = majority(sys.argv[2])
print(f"cases {len(b)}; cases whose three captures disagree: base {ub}, pr {up}")
d = [k for k in b if b[k][0] != p[k][0]]
print("base vs pr majority differences:", d)
weak = [k for k in b if b[k][1] < 2 or p[k][1] < 2]
print("cases without a 2 of 3 majority:", weak)
print(
    "allocating write_register calls: base",
    sum(1 for k in b if k.startswith("wreg") and b[k][2]),
    "pr",
    sum(1 for k in p if k.startswith("wreg") and p[k][2]),
)
