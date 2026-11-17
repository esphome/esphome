"""Per-connection phase times from a pcap of short HTTP requests (one request per connection).

usage: pcapphase.py PCAP DEVICE_IP [PORT]
For each client connection: accept (client SYN to device SYN-ACK), respond (client request data to the first
device data), close (first device data to device FIN). Prints median/p90 per phase in ms, plus SYN retries.
Needs tcpdump on PATH (on macOS it runs without sudo for members of the access_bpf group).
"""

import re
import statistics
import subprocess
import sys

pcap, dev = sys.argv[1], sys.argv[2]
port = sys.argv[3] if len(sys.argv) > 3 else "80"
proc = subprocess.run(
    ["tcpdump", "-nn", "-tt", "-r", pcap, f"tcp port {port}"],
    capture_output=True,
    text=True,
    check=False,
)
if proc.returncode:
    sys.exit(f"tcpdump failed ({proc.returncode}): {proc.stderr.strip()}")
out = proc.stdout
pat = re.compile(
    r"^(\d+\.\d+) IP (\S+)\.(\d+) > (\S+)\.(\d+): Flags \[([^\]]+)\].*?length (\d+)"
)
conns: dict = {}
for line in out.splitlines():
    m = pat.match(line)
    if not m:
        continue
    t, src, sp, dst, dp, flags, ln = (
        float(m[1]),
        m[2],
        m[3],
        m[4],
        m[5],
        m[6],
        int(m[7]),
    )
    from_dev = src == dev
    key = dp if from_dev else sp
    c = conns.setdefault(
        key, {"syn": [], "synack": None, "req": None, "resp": None, "fin": None}
    )
    if not from_dev and flags == "S":
        c["syn"].append(t)
    elif from_dev and flags == "S.":
        c["synack"] = c["synack"] or t
    elif not from_dev and ln > 0 and c["req"] is None:
        c["req"] = t
    elif from_dev and ln > 0 and c["resp"] is None:
        c["resp"] = t
    if from_dev and "F" in flags and c["fin"] is None:
        c["fin"] = t
phases: dict = {"accept": [], "respond": [], "close": []}
retries = 0
for c in conns.values():
    if not c["syn"]:
        continue
    retries += len(c["syn"]) - 1
    if c["synack"]:
        phases["accept"].append((c["synack"] - c["syn"][0]) * 1000)
    if c["req"] and c["resp"]:
        phases["respond"].append((c["resp"] - c["req"]) * 1000)
    if c["resp"] and c["fin"]:
        phases["close"].append((c["fin"] - c["resp"]) * 1000)
for k, v in phases.items():
    if len(v) > 2:
        print(
            f"{k:8} n {len(v):4} median {statistics.median(v):7.1f} ms  p90 {statistics.quantiles(v, n=10)[-1]:7.1f} ms  "
            f"mean {statistics.mean(v):7.1f} ms"
        )
print(f"connections {len(conns)}, SYN retransmits {retries}")
