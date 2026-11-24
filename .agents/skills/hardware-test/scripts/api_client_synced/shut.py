"""Per reboot, the trigger lines from just before the restart press to the ROM banner, with totals.
Usage: shut.py CAPTURE [FROM_TIME]   (FROM_TIME HH:MM:SS skips earlier reboots)"""

import bisect
import collections
import pathlib
import re
import sys

with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    lines = [line.rstrip("\n") for line in f]
start = sys.argv[2] if len(sys.argv) > 2 else "00:00:00"
KEEP = re.compile(
    r"Restarting device|Rebooting safely|ON_SHUTDOWN|SYNCED|DISCONNECTED|CONNECTED direct"
    r"|: (connected|disconnected)$|cap-|ESP-ROM"
)
idx = [
    i
    for i, line in enumerate(lines)
    if "Restarting device" in line and line[1:9] >= start
]
rom = [i for i, line in enumerate(lines) if "ESP-ROM" in line]
tot = collections.Counter()
for n, i in enumerate(idx):
    r = bisect.bisect_left(rom, i)
    j = rom[r] if r < len(rom) else len(lines)
    pre = [line for line in lines[max(0, i - 60) : i] if KEEP.search(line)][-8:]
    win = [line for line in lines[i : j + 1] if KEEP.search(line)]
    c = {
        "synced": sum("SYNCED direct" in line for line in win),
        "disc_direct": sum("DISCONNECTED direct" in line for line in win),
        "on_shutdown": sum("ON_SHUTDOWN" in line for line in win),
        "delayed/later": sum("delayed" in line or "cap-later" in line for line in win),
    }
    tot.update(c)
    print(
        f"--- reboot {n} at {lines[i][1:9]}:",
        " ".join(f"{k}={v}" for k, v in c.items()),
    )
    for line in pre:
        print("   pre ", line[:150])
    for line in win:
        print("   in  ", line[:150])
print("TOTAL", len(idx), dict(tot))
