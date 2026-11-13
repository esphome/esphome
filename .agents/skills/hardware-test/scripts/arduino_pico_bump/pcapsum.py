"""pcapsum.py PCAP DEVICE_IP: per TCP connection to port 80, device data segments, last data time and seq, retransmits,
FIN/RST and who sent them, windows, and anything sent by the device after its last data. Uses tcpdump -tt text."""

import re
import subprocess
import sys

pcap, dev = sys.argv[1], sys.argv[2]
txt = subprocess.run(
    ["tcpdump", "-n", "-tt", "-S", "-r", pcap],
    capture_output=True,
    text=True,
    check=False,
).stdout
rx = re.compile(
    r"^(\d+\.\d+) IP (\S+)\.(\d+) > (\S+)\.(\d+): Flags \[([^\]]*)\](?:, seq (\d+)(?::(\d+))?)?(?:, ack (\d+))?, win (\d+).*length (\d+)"
)
conns = {}
for line in txt.splitlines():
    m = rx.match(line)
    if not m:
        continue
    t, sip, sp, dip, dp, fl, s1, s2, ack, win, ln = m.groups()
    t = float(t)
    ln = int(ln)
    win = int(win)
    from_dev = sip == dev
    port = dp if from_dev else sp
    c = conns.setdefault(
        port,
        {
            "t0": t,
            "data": 0,
            "bytes": 0,
            "last_data": None,
            "last_seq": None,
            "seen": set(),
            "retx": 0,
            "fin": [],
            "rst": [],
            "cwin": [],
            "dwin": [],
            "after": [],
            "last_ack_from_client": None,
            "http": None,
        },
    )
    if "GET /" in line and not from_dev:
        c["http"] = line.split("HTTP: ")[-1][:20]
    if from_dev:
        c["dwin"].append(win)
        if ln > 0:
            key = (s1, s2)
            if key in c["seen"]:
                c["retx"] += 1
            c["seen"].add(key)
            c["data"] += 1
            c["bytes"] += ln
            c["last_data"] = t
            c["last_seq"] = int(s2)
            c["after"] = []
        else:
            c["after"].append((round(t - c["t0"], 3), fl))
    else:
        c["cwin"].append(win)
        if ack:
            c["last_ack_from_client"] = int(ack)
    if "F" in fl:
        c["fin"].append(("dev" if from_dev else "mac", round(t - c["t0"], 3)))
    if "R" in fl:
        c["rst"].append(("dev" if from_dev else "mac", round(t - c["t0"], 3)))
for p, c in conns.items():
    ld = round(c["last_data"] - c["t0"], 3) if c["last_data"] else None
    print(
        f"port {p} {c['http']}: dev data segs={c['data']} bytes={c['bytes']} last_data=+{ld}s last_dev_seq_end={c['last_seq']} "
        f"mac_last_ack={c['last_ack_from_client']} retx={c['retx']} FIN={c['fin']} RST={c['rst']} "
        f"mac_win=[{min(c['cwin'] or [0])},{max(c['cwin'] or [0])}] dev_win=[{min(c['dwin'] or [0])},{max(c['dwin'] or [0])}] "
        f"dev_ctrl_after_last_data={c['after'][:4]}"
    )
