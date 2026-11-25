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

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from lognorm import normalize  # noqa: E402

EV = re.compile(r"\[T\]: ([RSP]) (\d+) (.*)")
OTHER = {
    "status": "Range status",
    "timeout": "timed out",
    "startfail": "Starting measurement failed",
    "readfail": "Reading result failed",
    "still": "still running",
    "crash": r"Exception|wdt reset|rst cause|Soft WDT|ets Jan",
}
events = []
other = dict.fromkeys(OTHER, 0)
nr = nvalid = ns = 0
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    line = normalize(line)
    if m := EV.search(line):
        k, ms, v = m[1], int(m[2]), m[3]
        if k == "P":
            events.append((k, ms, v))
        else:
            try:
                v = float(v.split()[0])
            except (IndexError, ValueError):  # line cut by the serial log
                continue
            events.append((k, ms, v))
            nr += k == "R"
            nvalid += k == "R" and not math.isnan(v)
            ns += k == "S"
    for k, pat in OTHER.items():
        other[k] += bool(re.search(pat, line))

print(f"ranges {nr} valid {nvalid} nan {nr - nvalid} speeds {ns} other {other}")

prev = None  # last R event
base = None  # last valid R (baseline) if no NaN since
expect_speed = None
bad = []
first_after_invalid = 0
first_after_invalid_ok = 0
second_after_invalid_ok = 0
speeds_by_phase = {}
dts = {}
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
                dts.setdefault(phase, []).append(e[1] - base[1])
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
    d = dts[ph]
    print(
        f"phase {ph!r}: n={len(v)} mean={statistics.mean(v):+.4f} sd={statistics.pstdev(v):.4f} "
        f"min={min(v):+.4f} max={max(v):+.4f} dt ms min={min(d)} max={max(d)} mean={statistics.mean(d):.1f}"
    )
vr = [e[2] for e in events if e[0] == "R" and not math.isnan(e[2])]
if vr:
    print(
        f"range mean {statistics.mean(vr):.4f} sd {statistics.pstdev(vr) * 1000:.1f} mm min {min(vr)} max {max(vr)}"
    )
if bad or not nvalid or not ns or other["crash"]:
    sys.exit(
        f"FAIL: problems {len(bad)}, valid ranges {nvalid}, speeds {ns}, crashes {other['crash']}"
    )
