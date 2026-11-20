"""maj.py NAME_BASE NAME_PR: per case majority of rc and wire hash (reads: rc and verdict, since the chip's registers
change between runs) over NAME-ser.txt, -ser2 and -ser3 (1 to 3 captures), then base vs PR and the alloc summary."""

import argparse
from collections import Counter
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from snlog import first_alloc, is_read, runs  # noqa: E402


def majority(name: str) -> tuple[dict, int, int]:
    """Case -> (majority key, votes, max alloc); also the count of unstable cases and of captures."""
    caps = runs(name)
    out, unstable = {}, 0
    for k in caps[0]:
        c = Counter(
            (r[k]["rc"], r[k]["verdict"] if is_read(k) else r[k]["fnv"])
            for r in caps
            if k in r
        )
        v, n = c.most_common(1)[0]
        unstable += len(c) > 1
        out[k] = (v, n, max(r[k]["alloc"] for r in caps if k in r))
    return out, unstable, len(caps)


ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
ap.add_argument("base")
ap.add_argument("pr")
args = ap.parse_args()
b, ub, nb = majority(args.base)
p, up, np_ = majority(args.pr)
print(
    f"cases {len(b)} {len(p)}; captures base {nb}, pr {np_}; cases whose captures disagree: base {ub}, pr {up}"
)
d = [k for k in b if k not in p or b[k][0] != p[k][0]]
print("base vs pr majority differences:", len(d))
for k in d[:30]:
    print("  ", k, "base", b[k][0], "pr", p[k][0] if k in p else "missing")
weak = [k for k in b if 2 * b[k][1] <= nb or (k in p and 2 * p[k][1] <= np_)]
print("cases without a majority:", weak)
for name, maj in (("base", b), ("pr", p)):
    wr = [k for k in maj if k.startswith("wreg")]
    allocs = {k: v[2] for k, v in maj.items()}
    print(
        name,
        "write_register calls that allocated:",
        sum(allocs[k] > 0 for k in wr),
        "of",
        len(wr),
        "| first allocating len:",
        {s: first_alloc(allocs, s) for s in ("wreg8", "wreg16")},
        "| other allocs:",
        [k for k in maj if not k.startswith("wreg") and allocs[k]],
    )
