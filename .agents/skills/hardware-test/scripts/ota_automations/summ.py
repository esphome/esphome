"""Print the OTA automation fires of each case in order, progress fires collapsed to a count.

usage: summ.py OUT_DIR   (reads OUT_DIR/markers.txt and OUT_DIR/serial.txt written by run.sh)

Exits non-zero when a case's RESULT marker shows a failed step (upload rc, HTTP code, no progress before the kill,
pkill finding nothing) or a case lacks the automation that ends it.
"""

import argparse
from pathlib import Path
import re
import sys

# Expected RESULT text and the fire that must end each case (tag kind)
EXPECT = {
    "1": ("rc 0", "native end"),
    "2": ("progress yes pkill 0", "native error"),
    "3": ("http 200", "web error"),
    "4": ("http 200", "web end"),
}

DAY = 86400


class Clock:
    """HH:MM:SS to seconds that keep counting past midnight, for lines read in order."""

    def __init__(self) -> None:
        self.days = 0
        self.prev = None

    def __call__(self, hms: str) -> int:
        h, m, s = map(int, hms.split(":"))
        t = h * 3600 + m * 60 + s
        if self.prev is not None and t < self.prev - DAY // 2:
            self.days += 1
        self.prev = t
        return t + self.days * DAY


parser = argparse.ArgumentParser()
parser.add_argument("out", type=Path)
out = parser.parse_args().out
fires = []
serial_clock = Clock()
capture_start = None
for line in (out / "serial.txt").read_text(errors="replace").splitlines():
    if not (stamp := re.match(r"\[(\d\d:\d\d:\d\d)", line)):
        continue
    t = serial_clock(stamp[1])
    if capture_start is None:
        capture_start = t
    if m := re.search(r"AUTO (\w+) (\w+)\s*(-?[\d.]*)", line):
        fires.append((t, *m.groups()))
cases, results = [], {}
marker_clock = Clock()
for line in (out / "markers.txt").read_text().splitlines():
    t, text = line.split(" ", 1)
    if re.match(r"CASE\d ", text):
        cases.append((marker_clock(t), text))
    elif m := re.match(r"RESULT(\d) (.*)", text):
        results[m[1]] = m[2]
# The capture starts before the first marker; a first marker past midnight belongs to the next day
if cases and capture_start is not None and cases[0][0] + DAY // 2 < capture_start:
    cases = [(t + DAY, name) for t, name in cases]
failed = False
for i, (start, name) in enumerate(cases):
    end = cases[i + 1][0] if i + 1 < len(cases) else float("inf")
    seq, progress = [], 0
    for t, tag, kind, arg in fires:
        if not start <= t < end:
            continue
        if kind == "progress":
            progress += 1
        elif not (
            kind == "state" and arg == "2"
        ):  # state 2 repeats with every progress fire
            seq.append(f"{tag} {kind}{' ' + arg if arg else ''}")
    num = name[4]
    result = results.get(num, "missing")
    want_result, want_fire = EXPECT.get(num, (None, None))
    bad = []
    if want_result and result != want_result:
        bad.append(f"result {result!r}, expected {want_result!r}")
    if want_fire and not any(want_fire in f for f in seq):
        bad.append(f"no {want_fire!r}")
    failed |= bool(bad)
    print(f"{name} [{result}]: {' > '.join(seq)} (progress {progress})")
    for b in bad:
        print(f"  FAIL: {b}")
if len(cases) < len(EXPECT):
    print(f"FAIL: {len(cases)} of {len(EXPECT)} cases ran")
    failed = True
sys.exit(1 if failed else 0)
