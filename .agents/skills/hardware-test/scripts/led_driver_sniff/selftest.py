"""selftest.py CAPTURE: the known 57-bit pattern must come back exact, clock idling low and high, on both edges."""

import pathlib
import re
import sys

WANT = "".join(f"{b:08b}" for b in (0xA5, 0x3C, 0x00, 0xFF, 0x81, 0x7E, 0x12)) + "1"

text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
caps = re.findall(r"CAP selftest \S+ rise=(\d+) fall=(\d+) idle=(\d)", text)
rows = re.findall(r"\]: ([RF]) selftest \S+ ([0-9a-f]+)", text)
bad = []
if sorted(c[2] for c in caps) != ["0", "1"]:
    bad.append(f"want one selftest window per clock idle level, got {caps}")
if any(c[0] != "57" or c[1] != "57" for c in caps):
    bad.append(f"clock edge counts {caps}, want 57 rising and 57 falling")
if len(rows) != 4:
    bad.append(f"want 4 R/F bit lines, got {len(rows)}")
for edge, hexbits in rows:
    bits = "".join(f"{int(c, 16):04b}" for c in hexbits)[:57]
    if bits != WANT:
        bad.append(f"{edge} bits {bits} != {WANT}")
print("selftest FAIL: " + "; ".join(bad) if bad else "selftest PASS")
sys.exit(1 if bad else 0)
