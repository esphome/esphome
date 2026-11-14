"""check.py LOG [EXPECTED]: match the last 'STATE id = ...' line per text sensor in LOG against EXPECTED (expected.txt
from gen.py ... pr: id, tab, value; None means the sensor must never publish). Also checks the node 3 readings the box
model sends (ducosim.hpp node data: 21.5 °C, CO2 682, 54.32 %) and the mode and comfort writes the config drives
(CNT2 is 0x09, 22.3 °C goes out as 223 tenths), and fails on a request the box model flagged (bad CRC or no model
answer). Exits 1 on any mismatch."""

import pathlib
import re
import sys

log = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
exp = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else "expected.txt").read_text(
    encoding="utf-8"
)
seen = {m[1]: m[2] for m in re.finditer(r"STATE (\S+) = '(.*)' \(\d+ B\)", log)}
bad = 0
# numeric STATE lines: the last value per sensor id
nums = {
    m[1]: float(m[2])
    for m in re.finditer(r"STATE (\S+) = (-?[0-9.]+)\s*$", log, re.MULTILINE)
}
for sid, want in (("temperature", 21.5), ("co2", 682.0), ("humidity", 54.32)):
    got = nums.get(sid)
    ok = got is not None and abs(got - want) < 0.01
    bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {sid}: got {got} want {want}")
for line in ("Mode set to 09", "Comfort set to 223"):
    ok = line in log
    bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} box saw {line!r}")
for line in exp.splitlines():
    sid, want = line.split("\t", 1)
    got = seen.get(sid)
    ok = got is None if want == "None" else got == want
    bad += not ok
    print(f"{'ok  ' if ok else 'FAIL'} {sid}: got {got!r} want {want!r}")
flagged = re.findall(
    r"RX request (?:with a bad CRC|fn \S+ sub \S+ has no model answer)", log
)
for f in flagged[:5]:
    print(f"FAIL box model flagged: {f}")
bad += len(flagged)
if bad:
    sys.exit(f"FAIL: {bad} mismatches")
print("PASS")
