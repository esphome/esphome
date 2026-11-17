"""an15.py BUILD STA_MAC: per reboot, Disassociation copies the AP sniffer saw (grouped by sequence number),
burst length, and the station's reboot kind and join time."""

from collections import OrderedDict
import pathlib
import re
import sys

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
groups = OrderedDict()
for line in ap:
    m = re.search(
        r"MGMT (DISASSOC|DEAUTH) ta (\S+) .* seq (\d+) retry (\d) prot (\d) .* t_us (\d+)",
        line,
    )
    if m and m.group(2) == mac:
        g = groups.setdefault((m.group(1), m.group(3)), [])
        g.append((int(m.group(6)), int(m.group(4)), int(m.group(5))))
leaves = [
    re.search(r"reason (\d+)", line).group(1)
    for line in ap
    if "STA LEAVE" in line and mac in line
]
kinds = [
    re.search(r"REBOOT (\w+)", line).group(1)
    for line in st
    if "REBOOT" in line and "[st" in line
]
joins = [
    int(m.group(1)) for line in st if (m := re.search(r"JOINED after (\d+) ms", line))
]
disc = sum("Disconnecting before powerdown" in line for line in st)
crashes = [
    line for line in st if re.search(r"Guru|panic|abort\(\)|Backtrace|WDT", line)
]
copies = []
for (kind, seq), g in groups.items():
    t = [x[0] for x in g]
    copies.append(len(g))
    print(
        f"  {kind} seq {seq}: {len(g)} copies, {sum(x[1] for x in g)} with retry bit, prot {g[0][2]}, over {(max(t) - min(t)) / 1000:.1f} ms"
    )
print(
    f"{b}: reboots logged {len(kinds)} ({kinds.count('forced')} forced, {kinds.count('safe')} safe), "
    f"disassoc frames {len(groups)} copies {copies}, AP leave reasons {leaves}, "
    f"'Disconnecting before powerdown' {disc}, joins ms {joins}, crash lines {len(crashes)}"
)
