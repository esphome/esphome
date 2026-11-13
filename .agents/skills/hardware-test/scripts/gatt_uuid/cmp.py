"""cmp.py REF.json OTHER.json : list each UUID (service / characteristic / descriptor, in order) where OTHER differs
from REF, whether reads match (static characteristics) and whether every notifying characteristic notified.
Exits 1 on any difference."""

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


def chars(d: dict) -> list[dict]:
    return [c for s in d["services"] for c in s["characteristics"]]


# compare by position: a UUID form bug renames the keys, so values and notify counts are matched by table order
problems = []
rc, oc = chars(rd), chars(od)
for i, (a, b) in enumerate(zip(rc, oc, strict=False)):
    rv, ov = rd["reads"].get(a["uuid"]), od["reads"].get(b["uuid"])
    if str(ov).startswith("FAILED"):
        problems.append(f"read {i} {b['uuid']} {ov}")
    elif not a["props"] & 0x10 and rv is not None and ov != rv:
        problems.append(f"read {i} {b['uuid']} {ov!r} != ref {rv!r}")
    if a["props"] & 0x10:
        n = od["notify"].get(b["uuid"])
        if not isinstance(n, int) or n <= 0:
            problems.append(f"notify {i} {b['uuid']} {n}")
print(f"notify {od['notify']}")
for x in problems:
    print(" ", x)
if diff or len(ref) != len(oth) or problems:
    sys.exit(
        f"FAIL: {len(diff)} differing entries, {len(problems)} read or notify problems"
    )
print("PASS")
