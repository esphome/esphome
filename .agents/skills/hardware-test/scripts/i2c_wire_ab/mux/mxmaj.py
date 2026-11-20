"""mxmaj.py BASE PR: three captures each (NAME-ser.txt, -ser2, -ser3); per case the rc and stream hash seen in most
captures, base majority against PR majority, and the cases where any base and PR capture agree."""

from collections import Counter
import pathlib
import re
import sys


def load(path: str) -> dict[str, tuple]:
    cases = {}
    for line in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    ):
        if m := re.search(
            r"MX (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|fnv=([0-9A-F]+)\|", line
        ):
            cases[m.group(1)] = ((m.group(2), m.group(4)), int(m.group(3)))
    return cases


def runs(name: str) -> list[dict]:
    return [load(f"{name}-ser{s}.txt") for s in ("", "2", "3")]


B, P = runs(sys.argv[1]), runs(sys.argv[2])
keys = list(B[0])
nomaj, diff, shared = [], [], 0
for k in keys:
    cb = Counter(r[k][0] for r in B if k in r)
    cp = Counter(r[k][0] for r in P if k in r)
    if not cp:
        diff.append(k)
        continue
    (vb, nb), (vp, npr) = cb.most_common(1)[0], cp.most_common(1)[0]
    if nb < 2 or npr < 2:
        nomaj.append(k)
    elif vb != vp:
        diff.append(k)
    shared += bool(set(cb) & set(cp))
print(
    f"cases {len(keys)}; majority differs: {diff}; no 2 of 3 majority: {nomaj}; cases where captures agree: {shared}"
)
for name, R in (("base", B), ("pr", P)):
    wr = [k for k in keys if k.startswith("wreg")]
    per = [sum(r[k][1] > 0 for k in wr if k in r) for r in R]
    first = [
        next((x for x in range(301) if r.get(f"wreg8 len={x}", (0, 0))[1]), None)
        for r in R
    ]
    other = sorted({k for r in R for k in r if not k.startswith("wreg") and r[k][1]})
    print(
        f"{name}: allocating register writes per capture {per} of {len(wr)}; first wreg8 len {first}; other {other}"
    )
