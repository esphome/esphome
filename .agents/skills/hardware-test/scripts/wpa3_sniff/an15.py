"""an15.py BUILD STA_MAC: per reboot, Disassociation copies the AP sniffer saw (grouped by sequence number within one
burst), burst length, and the station's reboot kind and join time."""

import pathlib
import re
import sys

# copies of one frame arrive within tens of ms; reboots are 10 s or more apart, and the sequence counter restarts
BURST_GAP_US = 2_000_000

b, mac = sys.argv[1], sys.argv[2].upper()
ap = (
    pathlib.Path(f"{b}-ap.txt")
    .read_text(encoding="utf-8", errors="replace")
    .splitlines()
)
st = (
    pathlib.Path(f"{b}-st.txt")
    .read_text(encoding="utf-8", errors="replace")
    .splitlines()
)
groups: list[tuple[str, str, list[tuple[int, int, int]]]] = []
open_groups: dict[tuple[str, str], list[tuple[int, int, int]]] = {}
for line in ap:
    m = re.search(
        r"MGMT (DISASSOC|DEAUTH) ta (\S+) .* seq (\d+) retry (\d) prot (\d) .* t_us (\d+)",
        line,
    )
    if not m or m.group(2) != mac:
        continue
    key = (m.group(1), m.group(3))
    rec = (int(m.group(6)), int(m.group(4)), int(m.group(5)))
    g = open_groups.get(key)
    if g is None or abs(rec[0] - g[-1][0]) > BURST_GAP_US:
        g = open_groups[key] = []
        groups.append((*key, g))
    g.append(rec)
leaves = [
    m.group(1)
    for line in ap
    if "STA LEAVE" in line and mac in line and (m := re.search(r"reason (\d+)", line))
]
kinds = [
    m.group(1)
    for line in st
    if "[st" in line and (m := re.search(r"REBOOT (\w+)", line))
]
joins = [
    int(m.group(1)) for line in st if (m := re.search(r"JOINED after (\d+) ms", line))
]
disc = sum("Disconnecting before powerdown" in line for line in st)
crashes = sum(
    bool(re.search(r"Guru|panic|abort\(\)|Backtrace|WDT", line)) for line in st
)
copies = []
for kind, seq, g in groups:
    t = [x[0] for x in g]
    copies.append(len(g))
    print(
        f"  {kind} seq {seq}: {len(g)} copies, {sum(x[1] for x in g)} with retry bit, prot {g[0][2]}, "
        f"over {(max(t) - min(t)) / 1000:.1f} ms"
    )
print(
    f"{b}: reboots logged {len(kinds)} ({kinds.count('forced')} forced, {kinds.count('safe')} safe), "
    f"disassoc frames {len(groups)} copies {copies}, AP leave reasons {leaves}, "
    f"'Disconnecting before powerdown' {disc}, joins ms {joins}, crash lines {crashes}"
)
