"""mxmaj.py BASE PR: the captures NAME-ser.txt, -ser2 and -ser3 that exist (1 to 3); per case the rc and stream hash
seen in most captures, base majority against PR majority, and the cases where any base and PR capture agree."""

from collections import Counter
import pathlib
import re
import sys


def load(path: pathlib.Path) -> dict[str, tuple]:
    """Case -> ((rc, fnv), alloc, stream excerpt)."""
    cases = {}
    text = path.read_text(encoding="utf-8", errors="replace")
    if "MXDEAD" in text:
        sys.exit(f"{path}: the software target never answered")
    for line in text.splitlines():
        if m := re.search(
            r"MX (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|fnv=([0-9A-F]+)\|(.*)$", line
        ):
            cases[m[1]] = ((m[2], m[4]), int(m[3]), m[5].strip())
    return cases


def runs(name: str) -> list[dict]:
    found = [pathlib.Path(f"{name}-ser{s}.txt") for s in ("", "2", "3")]
    if not (loaded := [load(p) for p in found if p.exists()]):
        sys.exit(f"no capture {name}-ser.txt")
    if not all(loaded):
        sys.exit(f"a {name} capture has no MX cases")
    return loaded


B, P = runs(sys.argv[1]), runs(sys.argv[2])
# Every case seen in any capture, so a case one capture missed is still compared.
keys = list(dict.fromkeys(k for r in B + P for k in r))
nomaj, diff, shared = [], [], 0
for k in keys:
    cb = Counter(r[k][0] for r in B if k in r)
    cp = Counter(r[k][0] for r in P if k in r)
    if not cb or not cp:
        diff.append(k)
        continue
    (vb, nb), (vp, npr) = cb.most_common(1)[0], cp.most_common(1)[0]
    if 2 * nb <= len(B) or 2 * npr <= len(P):
        nomaj.append(k)
    elif vb != vp:
        diff.append(k)
    shared += bool(set(cb) & set(cp))
print(
    f"cases {len(keys)}; captures base {len(B)}, pr {len(P)}; majority differs: {len(diff)}; "
    f"no majority: {nomaj}; cases where captures agree: {shared}"
)
for k in diff[:10]:
    missing = (("?", ""), 0, "")
    b = next((r[k] for r in B if k in r), missing)
    p = next((r[k] for r in P if k in r), missing)
    print(f"  {k}\n     base {b[0][0]} {b[2][:120]}\n     pr   {p[0][0]} {p[2][:120]}")
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
