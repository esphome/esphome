"""check.py LOG [EXPECTED]: match the last 'STATE id = ...' line per text sensor in LOG against EXPECTED (expected.txt
from gen.py ... pr: id, tab, value; None means the sensor must never publish). Exits 1 on any mismatch."""

import pathlib
import re
import sys

log = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
exp = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else "expected.txt").read_text(
    encoding="utf-8"
)
seen = {m[1]: m[2] for m in re.finditer(r"STATE (\S+) = '(.*)' \(\d+ B\)", log)}
bad = 0
for line in exp.splitlines():
    sid, want = line.split("\t", 1)
    got = seen.get(sid)
    ok = got is None if want == "None" else got == want
    bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {sid}: got {got!r} want {want!r}")
if bad:
    sys.exit(f"FAIL: {bad} mismatches")
print("PASS")
