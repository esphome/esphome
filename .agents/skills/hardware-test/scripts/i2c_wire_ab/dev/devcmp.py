"""devcmp.py BASE_SER PR_SER: per case rc and wire hash must match (writes, probes); reads compare the verdict only,
since the chip's register contents change between runs. Prints diffs and the alloc summary."""

from collections import Counter
import pathlib
import re
import sys


def load(p):
    d = {}
    with pathlib.Path(p).open(encoding="utf-8", errors="replace") as fh:
        for line in fh:
            m = re.search(
                r"SN (.+?)\|rc=(-?\d+)\|alloc=(\d+)\|ev=\d+\|amb=(\d+)\|ovf=(\d+)\|fnv=([0-9A-F]+)\|(OK|DIFF)",
                line,
            )
            if m:
                d[m.group(1)] = (
                    int(m.group(2)),
                    int(m.group(3)),
                    m.group(6),
                    m.group(7),
                )
    return d


b, p = load(sys.argv[1]), load(sys.argv[2])
print("cases", len(b), len(p))
diffs = []
for k in b:
    if k not in p:
        diffs.append((k, "missing"))
        continue
    isread = k.startswith(("rreg", "read", "write1+read"))
    if (
        b[k][0] != p[k][0]
        or (not isread and b[k][2] != p[k][2])
        or (isread and b[k][3] != p[k][3])
    ):
        diffs.append((k, b[k], p[k]))
print("differences:", len(diffs))
for x in diffs[:30]:
    print("  ", x)
for name, d in (("base", b), ("pr", p)):
    wr = [k for k in d if k.startswith("wreg")]
    print(
        name,
        "write_register calls that allocated:",
        sum(d[k][1] > 0 for k in wr),
        "of",
        len(wr),
        "| first allocating len:",
        {
            s: next(
                (n for n in range(301) if d.get(f"{s} len={n}", (0, 0))[1] > 0), None
            )
            for s in ("wreg8", "wreg16")
        },
        "| verdicts:",
        dict(Counter(v[3] for v in d.values())),
        "| other allocs:",
        [k for k in d if not k.startswith("wreg") and d[k][1]],
    )
