"""an.py CAPTURE: error, warning and crash counts plus the heap probe summary of one flac-dut.yaml serial capture."""

import pathlib
import re
import sys

cap = (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
)


def count(pattern: str) -> int:
    return sum(1 for line in cap if re.search(pattern, line))


def field(line: str, key: str) -> int:
    return int(re.search(key + r" (\d+)", line).group(1))


print("errors [E]:", count(r"\]\[E\]"), " warnings [W]:", count(r"\]\[W\]"))
for line in cap:
    if re.search(r"\]\[[EW]\]", line) and "took a long time" not in line:
        print("  ", line[:170])
print("crashes:", count(r"Guru|abort\(\)|Backtrace"), " boots:", count(r"rst:0x"))
print("decoded info lines:", count(r"Decoded audio has"))
probes = [line for line in cap if "[probe" in line]
print("probes:", len(probes))
if not probes:
    sys.exit("no probe lines")
for key in ("int_free", "int_largest", "ps_free", "ps_largest"):
    xs = [field(line, key) for line in probes]
    print(f"{key}: first {xs[0]} min {min(xs)} max {max(xs)} last {xs[-1]}")
print("int_min (low water) last:", field(probes[-1], "int_min"))
