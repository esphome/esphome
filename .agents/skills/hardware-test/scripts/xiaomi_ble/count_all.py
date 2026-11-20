"""Count "PUB <dev>.<key> <value>" lines per entity in a capture, compare with expected_all.json and print a values
digest per entity so two builds can be compared. Usage: count_all.py CAPTURE [CAPTURE...]"""

import collections
import hashlib
import json
from pathlib import Path
import re
import sys

exp = json.loads(
    Path(__file__).with_name("expected_all.json").read_text(encoding="utf-8")
)
PUB = re.compile(r"PUB (\S+) (\S+)")
results = {}
failed = False
for cap in sys.argv[1:]:
    vals = collections.defaultdict(list)
    for line in Path(cap).read_text(encoding="utf-8", errors="replace").splitlines():
        if m := PUB.search(line):
            vals[m.group(1)].append(m.group(2))
    results[cap] = vals
    bad = 0
    print(f"== {cap}")
    for ent in sorted(set(exp) | set(vals)):
        got, want = len(vals.get(ent, [])), exp.get(ent)
        ok = got == want
        bad += not ok
        dig = hashlib.sha1(" ".join(vals.get(ent, [])).encode()).hexdigest()[:8]
        print(f"  {ent:26} want {want!s:>3} got {got:3} {dig} {'ok' if ok else 'DIFF'}")
    failed |= bad > 0
    print(
        f"  total publishes {sum(len(v) for v in vals.values())}, entities with a mismatch: {bad}"
    )
if len(results) > 1:
    caps = list(results)
    same = all(results[c] == results[caps[0]] for c in caps[1:])
    print("values identical across captures:", same)
    failed |= not same
if failed:
    sys.exit(1)
