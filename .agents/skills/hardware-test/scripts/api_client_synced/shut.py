"""Per reboot, the trigger lines from just before the restart press to the ROM banner, with totals.
Usage: shut.py CAPTURE [FROM_TIME]   (FROM_TIME HH:MM:SS skips earlier reboots)"""

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
tot = {
    "sync_in_shutdown": 0,
    "disc_direct": 0,
    "on_shutdown": 0,
    "later_in_shutdown": 0,
}
for n, i in enumerate(idx):
    j = next((k for k in range(i, len(lines)) if "ESP-ROM" in lines[k]), len(lines))
    pre = [line for line in lines[max(0, i - 60) : i] if KEEP.search(line)][-8:]
    win = [line for line in lines[i : j + 1] if KEEP.search(line)]
    s = sum("SYNCED direct" in line for line in win)
    d = sum("DISCONNECTED direct" in line for line in win)
    o = sum("ON_SHUTDOWN" in line for line in win)
    lt = sum("delayed" in line or "cap-later" in line for line in win)
    tot["sync_in_shutdown"] += s
    tot["disc_direct"] += d
    tot["on_shutdown"] += o
    tot["later_in_shutdown"] += lt
    print(
        f"--- reboot {n} at {lines[i][1:9]}: synced={s} disc_direct={d} on_shutdown={o} delayed/later={lt}"
    )
    for line in pre:
        print("   pre ", line[:150])
    for line in win:
        print("   in  ", line[:150])
print("TOTAL", len(idx), tot)
