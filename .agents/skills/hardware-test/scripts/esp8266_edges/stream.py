"""stream.py CAPTURE: split the bytes `jsn_sr04t` logged in the first 60 requests into frames and stray bytes.

A frame is `FF HI LO SUM` with SUM the low byte of FF + HI + LO.
"""

import pathlib
import re
import sys

requests = 0
bs = []
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
):
    if "Request read out" in line:
        requests += 1
        if requests > 60:
            break
    elif requests and (m := re.search(r"Read byte from sensor: ([0-9a-f]+)", line)):
        bs.append(int(m.group(1), 16))
i = good = 0
junk = []
while i < len(bs):
    if bs[i] == 0xFF and i + 3 < len(bs) and (sum(bs[i : i + 3]) & 0xFF) == bs[i + 3]:
        good += 1
        i += 4
    else:
        junk.append(bs[i])
        i += 1
print(
    f"bytes={len(bs)} frames={good} stray={len(junk)} "
    f"first_stray={[hex(x) for x in junk[:16]]}"
)
