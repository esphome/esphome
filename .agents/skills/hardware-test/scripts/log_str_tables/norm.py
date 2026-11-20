"""norm.py CAPTURE [NAME_RE]: print a capture with run to run noise removed, up to `MARK ALLDONE`.

Strips capture timestamps, log line numbers, the device name (NAME_RE, default `l\\d[pbq](66|32)`), the
compile time and timing values, then starts at the first ESPHome log line.
"""

import pathlib
import re
import sys

name_re = sys.argv[2] if len(sys.argv) > 2 else r"l\d[pbq](66|32)"
out = []
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for raw in f:
        line = re.sub(r"^\[\d\d:\d\d:\d\d(\.\d+)?\]", "", raw.rstrip("\n"))
        line = re.sub(r"\[([A-Z])\]\[([^:\]]+):\d+\]", r"[\1][\2]", line)
        line = re.sub(name_re, "NAME", line)
        line = re.sub(r"compiled on .*", "compiled on X", line)
        line = re.sub(
            r"took a long time for an operation \(\d+ ms\)",
            "took a long time (N ms)",
            line,
        )
        line = re.sub(r"\d+us after", "Nus after", line)
        line = re.sub(r"warming up, \d+ s left", "warming up, N s left", line)
        out.append(line)
        if "MARK ALLDONE" in line:
            break
start = next((i for i, ln in enumerate(out) if re.match(r"^\[[A-Z]\]\[", ln)), 0)
print("\n".join(out[start:]))
