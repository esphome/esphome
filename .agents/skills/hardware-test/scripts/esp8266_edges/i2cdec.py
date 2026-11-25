"""i2cdec.py CAPTURE: decode the first three `i2csniff.yaml` captures into start, stop, bytes and acks.

Each digit is SDA | SCL << 1 after a change. Prints `S`, `P`, and each byte as hex with `a` (ack) or `n` (nack).
"""

import pathlib
import re
import sys

caps = []
cur = ""
for line in (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="ignore").splitlines()
):
    m = re.search(r"seq(?::\d+)?\]: (\d+) (\d+)", line)
    if not m:
        continue
    if m.group(1) == "0":
        if cur:
            caps.append(cur)
        cur = ""
    cur += m.group(2)
if cur:
    caps.append(cur)
for c in caps[:3]:
    states = [3] + [int(x) for x in c]
    out = []
    bits = []
    for a, b in zip(states, states[1:], strict=False):
        sda_a, scl_a = a & 1, a >> 1
        sda_b, scl_b = b & 1, b >> 1
        if scl_a and scl_b and sda_a != sda_b:
            if bits:
                out.append("bits" + "".join(map(str, bits)))
            out.append("S" if sda_a else "P")
            bits = []
        elif not scl_a and scl_b:
            bits.append(sda_b)
            if len(bits) == 9:
                v = int("".join(map(str, bits[:8])), 2)
                out.append(f"{v:02X}{'n' if bits[8] else 'a'}")
                bits = []
    print(len(c), " ".join(out)[:1500])
    print()
