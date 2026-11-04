"""Per Wi-Fi drop timings from a serial capture of the outgoing connection timing config.

usage: wificycles.py CAPTURE [LABEL]
A cycle starts at "wifi cycle disabling"; the numbers are the device's millis() from the timing lines.
"dial" says whether the client that connected first came from the device's own dial ("Dialing" seen after the
reconnect and before the hello) or from an incoming connection ("Accept" seen first).
"""

import pathlib
import re
import statistics
import sys

cap = (
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace").splitlines()
)
label = sys.argv[2] if len(sys.argv) > 2 else ""
TS = re.compile(r"^\[(\d\d):(\d\d):(\d\d)(?:\.(\d+))?\]")


def secs(line):
    m = TS.match(line)
    if not m:
        return None
    h, mi, se, f = m.groups()
    return int(h) * 3600 + int(mi) * 60 + int(se) + (int(f) / 10 ** len(f) if f else 0)


cycles = []
cur = None
loop_max = []
for line in cap:
    m = re.search(r"loop time max (\d+) ms", line)
    if m:
        loop_max.append(int(m.group(1)))
    if "wifi cycle disabling" in line:
        cur = {"order": []}
        cycles.append(cur)
        continue
    if cur is None:
        continue
    m = re.search(r"wifi connected at (\d+) ms", line)
    if m and "wifi" not in cur:
        cur["wifi"] = int(m.group(1))
        cur["wifi_cap"] = secs(line)
    if "Dialing" in line:
        cur["order"].append("dial")
        cur["dials"] = cur.get("dials", 0) + 1
        if "dial_cap" not in cur and "wifi_cap" in cur:
            cur["dial_cap"] = secs(line)
        elif "dial_cap" not in cur and "dial_before_wifi_cap" not in cur:
            cur["dial_before_wifi_cap"] = secs(line)
    if (
        re.search(r"client disconnected at \d+ ms \(\d+\.\d+\.\d+\.\d+\)", line)
        and "wifi" in cur
        and "client" not in cur
    ):
        cur["dial_closed"] = cur.get("dial_closed", 0) + 1
    if "Accept " in line and "wifi" in cur:
        cur["order"].append("accept")
    m = re.search(r"client connected at (\d+) ms \((.*?)\)", line)
    if m and "wifi" in cur and "client" not in cur:
        cur["client"] = int(m.group(1))
        cur["who"] = m.group(2).split(" ")[0]
        cur["via"] = cur["order"][0] if cur["order"] else "?"
    m = re.search(r"Connect (failed|timeout)", line)
    if m:
        cur.setdefault("connect_errors", 0)
        cur["connect_errors"] += 1

print(f"{label} cycles: {len(cycles)}")
print(
    f"{'n':>2} {'wifi ms':>8} {'dial-wifi cap ms':>16} {'client ms':>10} {'client-wifi':>11}  via     dials closed  client"
)
for i, c in enumerate(cycles, 1):
    cw = c["client"] - c["wifi"] if "client" in c and "wifi" in c else ""
    c["client-wifi"] = cw
    dw = ""
    if "wifi_cap" in c and c.get("wifi_cap") is not None:
        # the first dial of the cycle, whether it was logged before or after the Wi-Fi line
        if c.get("dial_before_wifi_cap") is not None:
            dw = f"{(c['dial_before_wifi_cap'] - c['wifi_cap']) * 1000:.0f}"
        elif "dial_cap" in c and c["dial_cap"] is not None:
            dw = f"{(c['dial_cap'] - c['wifi_cap']) * 1000:.0f}"
    c["dial-wifi"] = dw
    print(
        f"{i:>2} {c.get('wifi', ''):>8} {dw:>16} {c.get('client', ''):>10} {cw:>11}  {c.get('via', ''):7s} {c.get('dials', 0):>5} {c.get('dial_closed', 0):>6}  {c.get('who', '')}"
    )
for via in ("dial", "accept"):
    vals = [
        c["client-wifi"]
        for c in cycles
        if c.get("via") == via and c["client-wifi"] != ""
    ]
    if vals:
        print(
            f"client-wifi via {via}: n={len(vals)} min={min(vals)} median={statistics.median(vals):.0f} max={max(vals)}"
        )
dws = [float(c["dial-wifi"]) for c in cycles if c.get("dial-wifi") not in ("", None)]
if dws:
    print(
        f"dial after wifi connected (capture clock): n={len(dws)} min={min(dws):.0f} median={statistics.median(dws):.0f} max={max(dws):.0f}"
    )
if loop_max:
    print(
        f"loop time max per 5 s: n={len(loop_max)} min={min(loop_max)} median={statistics.median(loop_max):.0f} max={max(loop_max)}"
    )
