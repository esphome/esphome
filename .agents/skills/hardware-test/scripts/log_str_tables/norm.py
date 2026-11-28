"""norm.py CAPTURE [NAME_RE]: print a capture with run to run noise removed, up to `MARK ALLDONE`.

Strips what `lognorm.py` strips, the device name (NAME_RE, default `l\\d[pbq](66|32)`), the compile time and
timing values, then starts at the first ESPHome log line.
"""

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from lognorm import normalize  # noqa: E402

name_re = re.compile(sys.argv[2] if len(sys.argv) > 2 else r"l\d[pbq](66|32)")
NOISE = (
    (re.compile(r"compiled on .*"), "compiled on X"),
    (
        re.compile(r"took a long time for an operation \(\d+ ms\)"),
        "took a long time (N ms)",
    ),
    (re.compile(r"\d+us after"), "Nus after"),
    (re.compile(r"warming up, \d+ s left"), "warming up, N s left"),
)
out = []
with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    for raw in f:
        line = name_re.sub("NAME", normalize(raw.rstrip("\n")))
        for pat, repl in NOISE:
            line = pat.sub(repl, line)
        out.append(line)
        if "MARK ALLDONE" in line:
            break
    else:
        sys.exit(f"{sys.argv[1]}: no MARK ALLDONE, the scripted test did not finish")
start = next((i for i, ln in enumerate(out) if re.match(r"^\[[A-Z]\]\[", ln)), 0)
print("\n".join(out[start:]))
