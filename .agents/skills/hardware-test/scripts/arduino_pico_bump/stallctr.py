"""stallctr.py PCAP DEV_IP APILOG: for each /events connection print last data and FIN wall times, and the atcp counter
line just before the last data, just after it (about +1 s), and about 6 s later (after the FIN)."""

import pathlib
import re
import subprocess
import sys

pcap, dev, logf = sys.argv[1:4]
txt = subprocess.run(
    ["tcpdump", "-n", "-tt", "-r", pcap], capture_output=True, text=True, check=False
).stdout
conns = {}
for line in txt.splitlines():
    m = re.match(
        r"^(\d+\.\d+) IP (\S+)\.(\d+) > (\S+)\.(\d+): Flags \[([^\]]*)\].*length (\d+)",
        line,
    )
    if not m:
        continue
    t, s, sp, d, dp, fl, ln = m.groups()
    t = float(t)
    port = dp if s == dev else sp
    c = conns.setdefault(port, {"t0": t, "ev": False, "last": None, "fin": None})
    if "GET /events" in line:
        c["ev"] = True
    if s == dev and int(ln) > 0:
        c["last"] = t
    if s == dev and "F" in fl and c["fin"] is None:
        c["fin"] = t
logs = []
for line in pathlib.Path(logf).read_text(encoding="utf-8").splitlines():
    if "[atcp" in line:
        t, rest = line.split(" ", 1)
        logs.append(
            (float(t), re.sub(r"\x1b\[[0-9;]*m", "", rest).strip().split("]: ", 1)[-1])
        )


def at(t):
    best = None
    for lt, s in logs:
        if lt <= t:
            best = (lt, s)
    return best


for p, c in conns.items():
    if not c["ev"] or c["last"] is None:
        continue
    fin = (
        f"FIN dev +{c['fin'] - c['last']:.2f}s after last data"
        if c["fin"]
        else "no dev FIN"
    )
    print(f"conn {p}: last data at +{c['last'] - c['t0']:.2f}s, {fin}")
    for lbl, t in (
        ("before", c["last"] - 0.01),
        ("+1.5s", c["last"] + 1.5),
        ("+7s", c["last"] + 7),
    ):
        a = at(t)
        if a:
            print(f"   {lbl:7} {a[1]}")
