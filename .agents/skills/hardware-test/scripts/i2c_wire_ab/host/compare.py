"""compare.py BASE PR: wire and return code per bus and case must match; summarize allocations and IDF job lists."""

from collections import Counter, defaultdict
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
from snlog import first_alloc  # noqa: E402


def load(p):
    rec, jobs = {}, {}
    with pathlib.Path(p).open(encoding="utf-8") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("ALLOC_PROBE"):
                continue
            parts = line.split("|")
            if parts[0] == "J":
                jobs[(parts[1], parts[2])] = parts[3].strip()
                continue
            bus, case, rc, alloc, wire = parts
            rec[(bus, case)] = (rc, int(alloc.split("=")[1]), wire.strip())
    return rec, jobs


b, bj = load(sys.argv[1])
p, pj = load(sys.argv[2])
assert b.keys() == p.keys(), "case lists differ"
diff = [k for k in b if b[k][0] != p[k][0] or b[k][2] != p[k][2]]
per_bus = Counter(k[0] for k in b)
print("cases per bus:", dict(per_bus))
print("wire/rc differences:", len(diff))
for k in diff[:40]:
    print(
        "  DIFF",
        k,
        "\n    base",
        b[k][0],
        b[k][2][:160],
        "\n    pr  ",
        p[k][0],
        p[k][2][:160],
    )
alloc = defaultdict(
    lambda: [0, 0, 0, 0]
)  # base cases with alloc, pr cases with alloc, base total, pr total
for k in b:
    if not k[1].startswith(("wreg8", "wreg16")):
        continue
    a = alloc[k[0]]
    a[0] += b[k][1] > 0
    a[1] += p[k][1] > 0
    a[2] += b[k][1]
    a[3] += p[k][1]
print("write_register/16 calls that allocated (base, pr):")
for bus, a in alloc.items():
    print(
        f"  {bus:8s} base {a[0]} calls ({a[2]} mallocs)  pr {a[1]} calls ({a[3]} mallocs)"
    )
# first length that allocates, per bus and shape
for bus in per_bus:
    ab = {c: r[1] for (x, c), r in b.items() if x == bus}
    ap = {c: r[1] for (x, c), r in p.items() if x == bus}
    for shape in ("wreg8", "wreg16"):
        fb, fp = first_alloc(ab, shape), first_alloc(ap, shape)
        print(f"  first allocating length {bus} {shape}: base {fb} pr {fp}")
other = [
    k
    for k in b
    if not k[1].startswith(("wreg8", "wreg16", "wedge wreg", "absent wreg"))
    and (b[k][1] or p[k][1])
]
print("other calls with allocations:", other[:10])
jd = Counter()
for k in bj:
    if bj[k] != pj.get(k):
        jd[(bj[k], pj.get(k))] += 1
print("IDF job list changes (base -> pr: count):")
for (x, y), n in sorted(jd.items(), key=lambda t: -t[1])[:20]:
    print(f"  {x!r} -> {y!r}: {n}")
