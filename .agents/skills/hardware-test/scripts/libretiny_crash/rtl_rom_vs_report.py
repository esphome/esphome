"""Match each RTL8720C ROM fault dump in a serial capture with the report on the next boot.

usage: rtl_rom_vs_report.py SERIAL_CAPTURE   (taken with serial_capture.py --ms)
Per ROM dump: the ROM's CFSR and PC, ms from the dump to the next `ROM Version` banner, and the Reason, CFSR, PC and SP
the next boot reported. The ROM lines start with a bare CR, so the capture is split on newlines only.
"""

import pathlib
import re
import sys

RAW = pathlib.Path(sys.argv[1]).read_bytes().decode(errors="replace")
LINES = [
    re.sub(r"\x1b\[[0-9;]*m", "", line) for line in RAW.replace("\r", "").split("\n")
]


def stamp(line: str) -> float | None:
    """Seconds since midnight from a serial_capture.py --ms timestamp."""
    m = re.search(r"\[(\d\d):(\d\d):(\d\d\.\d{3})\]", line)
    return (
        int(m.group(1)) * 3600 + int(m.group(2)) * 60 + float(m.group(3)) if m else None
    )


i = 0
while i < len(LINES):
    if "Fault Handler:" not in LINES[i]:
        i += 1
        continue
    t0 = stamp(LINES[i])
    kind = LINES[i + 2].split("]", 1)[-1].strip() if i + 2 < len(LINES) else "?"
    cfsr = pc = None
    j = i
    while j < len(LINES) and "ROM Version" not in LINES[j]:
        if cfsr is None and "Configurable Fault Status Reg" in LINES[j]:
            cfsr = LINES[j].split("=")[1].strip()
        if pc is None and "PC  =" in LINES[j]:
            pc = LINES[j].split("=")[1].strip()
        j += 1
    tj = next(
        (
            stamp(LINES[x])
            for x in range(j, min(j + 5, len(LINES)))
            if stamp(LINES[x]) is not None
        ),
        None,
    )
    reset = (
        f"{(tj - t0) * 1000:.0f} ms"
        if tj is not None and t0 is not None
        else "no reset seen"
    )
    report = {}
    k = j + 1
    while (
        k < len(LINES)
        and "Fault Handler:" not in LINES[k]
        and "boot reason" not in LINES[k]
    ):
        m = re.search(r"(Reason|CFSR|PC|SP):\s+(.*)", LINES[k])
        if m and "libretiny.crash" in LINES[k]:
            report.setdefault(m.group(1), m.group(2).split("  (")[0].strip())
        k += 1
    print(f"{kind:18} ROM CFSR {cfsr} PC {pc} | reset {reset} | report {report}")
    i = j
