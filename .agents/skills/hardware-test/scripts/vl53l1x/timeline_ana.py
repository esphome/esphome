"""timeline_ana.py CAPTURE: check speed publishes against consecutive logged ranges (esphome#23108).

Reads the R (range), S (speed) and P (phase) lines timeline.yaml logs. Every valid range after a valid one must be
followed by a speed equal to the range change over the time between them; no speed after a NaN or on the first valid
range after one. Prints per phase speed stats and the problems found.
"""

import math
import pathlib
import re
import statistics
import sys

R = re.compile(r"\[T(?::\d+)?\]: R (\d+) (\S+)")
S = re.compile(r"\[T(?::\d+)?\]: S (\d+) (\S+)")
P = re.compile(r"\[T(?::\d+)?\]: P (\d+) (.*)")
events = []
other = {
    "status": 0,
    "timeout": 0,
    "startfail": 0,
    "readfail": 0,
    "still": 0,
    "crash": 0,
}
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    if m := R.search(line):
        events.append(("R", int(m[1]), float(m[2])))
    elif m := S.search(line):
        events.append(("S", int(m[1]), float(m[2])))
    elif m := P.search(line):
        events.append(("P", int(m[1]), m[2]))
    if "Range status" in line:
        other["status"] += 1
    if "timed out" in line:
        other["timeout"] += 1
    if "Starting measurement failed" in line:
        other["startfail"] += 1
    if "Reading result failed" in line:
        other["readfail"] += 1
    if "still running" in line:
        other["still"] += 1
    if re.search(r"Exception|wdt reset|rst cause|Soft WDT|ets Jan", line):
        other["crash"] += 1

nr = sum(1 for e in events if e[0] == "R")
nvalid = sum(1 for e in events if e[0] == "R" and not math.isnan(e[2]))
ns = sum(1 for e in events if e[0] == "S")
print(f"ranges {nr} valid {nvalid} nan {nr - nvalid} speeds {ns} other {other}")

prev = None  # last R event
base = None  # last valid R (baseline) if no NaN since
expect_speed = None
bad = []
first_after_invalid = 0
first_after_invalid_ok = 0
second_after_invalid_ok = 0
speeds_by_phase = {}
dts = []
phase = "start"
max_err = 0.0
after_invalid = False
last_was_first = False
for i, e in enumerate(events):
    if e[0] == "P":
        phase = e[2]
        continue
    if e[0] == "R":
        # speed (if any) follows this R in the same loop tick
        nxt = events[i + 1] if i + 1 < len(events) else None
        got = nxt if nxt and nxt[0] == "S" and nxt[1] == e[1] else None
        if math.isnan(e[2]):
            if got:
                bad.append(("speed after NaN", e, got))
            base = None
            after_invalid = True
            last_was_first = False
            continue
        if base is None:
            if prev is not None and after_invalid:
                first_after_invalid += 1
                if got is None:
                    first_after_invalid_ok += 1
                last_was_first = True
            if got:
                bad.append(("speed without baseline", e, got))
        else:
            dt = (e[1] - base[1]) / 1000.0
            exp = (e[2] - base[2]) / dt
            if got is None:
                bad.append(("missing speed", e))
            else:
                err = abs(got[2] - exp)
                max_err = max(max_err, err)
                if err > 1e-4:
                    bad.append(("wrong speed", base, e, got, exp))
                speeds_by_phase.setdefault(phase, []).append(got[2])
                dts.append((phase, e[1] - base[1]))
                if last_was_first:
                    second_after_invalid_ok += 1
            last_was_first = False
        after_invalid = False
        base = e
        prev = e
    elif e[0] == "S" and not (
        i > 0 and events[i - 1][0] == "R" and events[i - 1][1] == e[1]
    ):
        bad.append(("orphan speed", e))

print(
    f"first valid after invalid: {first_after_invalid}, without speed: {first_after_invalid_ok}, "
    f"next valid with speed: {second_after_invalid_ok}"
)
print(f"max |speed - dRange/dt| = {max_err:.2e} m/s; problems {len(bad)}")
for b in bad[:20]:
    print("  ", b)
for ph, v in speeds_by_phase.items():
    d = [x for p, x in dts if p == ph]
    print(
        f"phase {ph!r}: n={len(v)} mean={statistics.mean(v):+.4f} sd={statistics.pstdev(v):.4f} "
        f"min={min(v):+.4f} max={max(v):+.4f} dt ms min={min(d)} max={max(d)} mean={statistics.mean(d):.1f}"
    )
vr = [e[2] for e in events if e[0] == "R" and not math.isnan(e[2])]
if vr:
    print(
        f"range mean {statistics.mean(vr):.4f} sd {statistics.pstdev(vr) * 1000:.1f} mm min {min(vr)} max {max(vr)}"
    )
