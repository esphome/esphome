"""an15.py BUILD STA_MAC: per reboot, Disassociation copies the AP sniffer saw (grouped by sequence number within one
burst), burst length, and the station's reboot kind and join time."""

import pathlib
import re
import sys

# copies of one frame arrive within tens of ms; reboots are 10 s or more apart, and the sequence counter restarts
BURST_GAP_US = 2_000_000

b, mac = sys.argv[1], sys.argv[2].upper()
if not re.fullmatch(r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}", mac):
    sys.exit(f"STA_MAC must look like AA:BB:CC:DD:EE:FF, got {mac!r}")
paths = [pathlib.Path(f"{b}-ap.txt"), pathlib.Path(f"{b}-st.txt")]
if missing := [str(x) for x in paths if not x.is_file()]:
    sys.exit(f"missing capture {', '.join(missing)}")
ap, st = (x.read_text(encoding="utf-8", errors="replace").splitlines() for x in paths)
# zero Disassociation frames is a valid result (deep sleep sends none), but not with a dead sniffer, a station the AP
# never saw or a station that never rebooted
if not any("MGMT " in line for line in ap):
    sys.exit(f"{b}: no MGMT lines in the AP capture; the sniffer did not run")
if not any(mac in line for line in ap):
    sys.exit(f"{b}: the AP capture never mentions {mac}; check the station MAC")
if not any("REBOOT" in line for line in st):
    sys.exit(f"{b}: no REBOOT line in the station capture")
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
# the stations reboot on purpose (SW_CPU, deep sleep) and log warnings while they rejoin; error lines, crash
# markers and any other reset reason (watchdog, panic, brownout) are device trouble on either side
CRASH = re.compile(r"\[E\]\[|Guru|panic|abort\(\)|Backtrace|WDT|Brownout")
OK_RESET = re.compile(
    r"rst:0x[0-9a-f]+ \((?:SW_CPU(?:_RESET)?|POWERON(?:_RESET)?|DEEPSLEEP(?:_RESET)?|DSLEEP)\)"
)
bad_lines = [
    f"{side}: {line.strip()}"
    for side, lines in (("ap", ap), ("st", st))
    for line in lines
    if CRASH.search(line) or ("rst:" in line and not OK_RESET.search(line))
]
crashes = len(bad_lines)
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
for line in bad_lines[:20]:
    print("  BAD", line[:160])
sys.exit(1 if bad_lines else 0)
