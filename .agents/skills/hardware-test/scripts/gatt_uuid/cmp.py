"""cmp.py REF.json OTHER.json : list each UUID (service / characteristic / descriptor, in order) where OTHER differs
from REF, and whether reads and notifications worked. Exits 1 on any difference or failed read."""

import json
import pathlib
import sys


def flat(p):
    d = json.loads(pathlib.Path(p).read_text(encoding="utf-8"))
    rows = []
    for s in d["services"]:
        rows.append(("svc", s["uuid"].lower()))
        for c in s["characteristics"]:
            rows.append(("chr", c["uuid"].lower()))
            rows += [("dsc", x.lower()) for x in c["descriptors"]]
    return d, rows


rd, ref = flat(sys.argv[1])
od, oth = flat(sys.argv[2])
diff = [(a, b) for a, b in zip(ref, oth, strict=False) if a != b]
print(
    f"entries ref={len(ref)} other={len(oth)} differing={len(diff) + abs(len(ref) - len(oth))}"
)
for a, b in diff:
    print(f"  {a[0]} ref {a[1]}  other {b[1]}")
reads = od["reads"]
print(
    f"reads ok={sum(not str(v).startswith('FAILED') for v in reads.values())}/{len(reads)} "
    f"same values as ref by position={[*reads.values()][-4:] == [*rd['reads'].values()][-4:]}"
)
print(f"notify {od['notify']}")
failed = [u for u, v in reads.items() if str(v).startswith("FAILED")]
if diff or len(ref) != len(oth) or failed:
    sys.exit(f"FAIL: {len(diff)} differing entries, {len(failed)} failed reads")
print("PASS")
