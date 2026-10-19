"""Per wake cycle timings from a serial capture of the deep sleep timing config.

usage: cycles.py CAPTURE [LABEL]
A cycle starts at the boot banner; the numbers are the device's millis() from the timing lines, and the
capture clock gap from the first line of the cycle to the "Dialing" line.
"""

import pathlib
import re
import statistics
import sys

cap = (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
)
label = sys.argv[2] if len(sys.argv) > 2 else ""
TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)(?:\.(\d+))?\]")


def secs(line):
    m = TS.match(line)
    if not m:
        return None
    h, mi, s, f = m.groups()
    return int(h) * 3600 + int(mi) * 60 + int(s) + (int(f) / 10 ** len(f) if f else 0)


cycles = []
cur = None
for line in cap:
    if "rst:0x" in line or (cur is None and "Log initialized" in line):
        cur = {"start": secs(line), "reset": line.split("]", 1)[-1].strip()[:60]}
        cycles.append(cur)
        continue
    if cur is None:
        continue
    if "Dialing" in line:
        cur.setdefault("dial_cap", secs(line))
    m = re.search(r"wifi connected at (\d+) ms", line)
    if m:
        cur.setdefault("wifi", int(m.group(1)))
    m = re.search(r"client connected at (\d+) ms \((.*?)\)", line)
    if m and "client" not in cur:
        cur["client"] = int(m.group(1))
        cur["who"] = m.group(2)
    m = re.search(r"first HA state '(.*?)' at (\d+) ms", line)
    if m and "state" not in cur:
        cur["state"] = int(m.group(2))

print(f"{label} cycles: {len(cycles)}")
print(
    f"{'n':>2} {'wifi ms':>8} {'dial +ms':>9} {'client ms':>10} {'state ms':>9} {'client-wifi':>11} {'state-client':>12}  client"
)
for i, c in enumerate(cycles, 1):
    dial = ""
    if c.get("dial_cap") is not None and c.get("start") is not None:
        dial = f"{(c['dial_cap'] - c['start']) * 1000:.0f}"
    cw = c["client"] - c["wifi"] if "client" in c and "wifi" in c else ""
    sc = c["state"] - c["client"] if "state" in c and "client" in c else ""
    print(
        f"{i:>2} {c.get('wifi', ''):>8} {dial:>9} {c.get('client', ''):>10} {c.get('state', ''):>9} {cw:>11} {sc:>12}  {c.get('who', '')}"
    )
for c in cycles:
    if "client" in c and "wifi" in c:
        c["client-wifi"] = c["client"] - c["wifi"]
    if "state" in c and "client" in c:
        c["state-client"] = c["state"] - c["client"]
for key in ("wifi", "client", "state", "client-wifi", "state-client"):
    vals = [c[key] for c in cycles if key in c]
    if vals:
        print(
            f"{key}: n={len(vals)} min={min(vals)} median={statistics.median(vals):.0f} max={max(vals)}"
        )
