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

parser = argparse.ArgumentParser()
parser.add_argument("out", type=Path)
out = parser.parse_args().out
cases, results = [], {}
for line in (out / "markers.txt").read_text().splitlines():
    t, text = line.split(" ", 1)
    if re.match(r"CASE\d ", text):
        cases.append((t, text))
    elif m := re.match(r"RESULT(\d) (.*)", text):
        results[m[1]] = m[2]
fires = []
for line in (out / "serial.txt").read_text(errors="replace").splitlines():
    m = re.match(r"\[(\d\d:\d\d:\d\d)[^ ]*.*AUTO (\w+) (\w+)\s*(-?[\d.]*)", line)
    if m:
        fires.append(m.groups())
failed = False
for i, (start, name) in enumerate(cases):
    end = cases[i + 1][0] if i + 1 < len(cases) else "99"
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
