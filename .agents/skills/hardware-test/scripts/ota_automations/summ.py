"""Print the OTA automation fires of each case in order, progress fires collapsed to a count.

usage: summ.py OUT_DIR   (reads OUT_DIR/markers.txt and OUT_DIR/serial.txt written by run.sh)
"""

import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument("out", type=Path)
out = parser.parse_args().out
cases = []
for line in (out / "markers.txt").read_text().splitlines():
    t, text = line.split(" ", 1)
    if re.match(r"CASE\d ", text):
        cases.append((t, text))
fires = []
for line in (out / "serial.txt").read_text(errors="replace").splitlines():
    m = re.match(r"\[(\d\d:\d\d:\d\d)[^ ]*.*AUTO (\w+) (\w+)\s*(-?[\d.]*)", line)
    if m:
        fires.append(m.groups())
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
    print(f"{name}: {' > '.join(seq)} (progress {progress})")
