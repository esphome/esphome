"""Per AP drop, seconds from the drop to the DUT's upload end, disconnect, first scan or connect, and reconnect.
Usage: dropana.py RELAY_LOG DUT_LOG
RELAY_LOG is relay_host.py's output (DROP lines), DUT_LOG a serial_capture.py log of dut-sta.yaml; both stamped
by the same host clock."""

import pathlib
import re
import sys


def t(s: str) -> int:
    h, m, sec = (int(x) for x in s.split(":"))
    return h * 3600 + m * 60 + sec


with pathlib.Path(sys.argv[1]).open(encoding="utf-8", errors="replace") as f:
    drops = [line[:8] for line in f if "DROP:" in line]
lines = []
with pathlib.Path(sys.argv[2]).open("rb") as f:
    for raw in f:
        line = raw.decode("utf-8", "replace")
        if re.match(r"\[\d\d:\d\d:\d\d", line):
            lines.append((line[1:9], line.rstrip()))
pats = {
    "upload_end": r"OTAEV (web )?(end|abort|error)|OTA upload interrupted|Update complete",
    "disc": r"WIFIEV disconnected",
    "retry": r"Starting scan|Connecting to ",
    "conn": r"WIFIEV connected",
}
for d in drops:
    out = {}
    for ts, line in lines:
        if t(ts) < t(d) or t(ts) - t(d) > 200:
            continue
        for k, p in pats.items():
            if k in out or not (m := re.search(p, line)):
                continue
            if k == "retry" and "disc" not in out:
                continue
            out[k] = t(ts) - t(d)
            if k == "upload_end":
                out["upload_how"] = m.group(0)
    keys = ["upload_end", "upload_how", "disc", "retry", "conn"]
    print(d, " ".join(f"{k}={out.get(k)}" for k in keys))
