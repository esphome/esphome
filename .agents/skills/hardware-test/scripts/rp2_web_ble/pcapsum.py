"""pcapsum.py PCAP DEVICE_IP: per TCP connection to port 80, device data segments, last data time and seq, retransmits,
FIN/RST and who sent them, windows, and anything sent by the device after its last data. Uses tcpdump -tt text.
packets() is shared with stallctr.py."""

from collections.abc import Iterator
import re
import subprocess
import sys

RX = re.compile(
    r"^(\d+\.\d+) IP (\S+)\.(\d+) > (\S+)\.(\d+): Flags \[([^\]]*)\](?:, seq (\d+)(?::(\d+))?)?(?:, ack (\d+))?, win (\d+).*length (\d+)"
)


def packets(pcap: str, dev: str) -> Iterator[dict]:
    """Each TCP packet in PCAP as a dict; port is the host side's port, from_dev tells the direction."""
    r = subprocess.run(
        ["tcpdump", "-n", "-tt", "-S", "-r", pcap],
        capture_output=True,
        text=True,
        check=False,
    )
    if r.returncode != 0:
        sys.exit(f"tcpdump -r {pcap} failed: {r.stderr.strip()}")
    for line in r.stdout.splitlines():
        m = RX.match(line)
        if not m:
            continue
        t, sip, sp, _dip, dp, fl, s1, s2, ack, win, ln = m.groups()
        from_dev = sip == dev
        yield {
            "t": float(t),
            "from_dev": from_dev,
            "port": dp if from_dev else sp,
            "flags": fl,
            "seq": (s1, s2),
            "ack": int(ack) if ack else None,
            "win": int(win),
            "len": int(ln),
            "line": line,
        }


def main(pcap: str, dev: str) -> None:
    conns: dict[str, dict] = {}
    for k in packets(pcap, dev):
        t = k["t"]
        c = conns.setdefault(
            k["port"],
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
        if "GET /" in k["line"] and not k["from_dev"]:
            c["http"] = k["line"].split("HTTP: ")[-1][:20]
        if k["from_dev"]:
            c["dwin"].append(k["win"])
            if k["len"] > 0:
                if k["seq"] in c["seen"]:
                    c["retx"] += 1
                c["seen"].add(k["seq"])
                c["data"] += 1
                c["bytes"] += k["len"]
                c["last_data"] = t
                c["last_seq"] = int(k["seq"][1])
                c["after"] = []
            else:
                c["after"].append((round(t - c["t0"], 3), k["flags"]))
        else:
            c["cwin"].append(k["win"])
            if k["ack"] is not None:
                c["last_ack_from_client"] = k["ack"]
        who = "dev" if k["from_dev"] else "host"
        if "F" in k["flags"]:
            c["fin"].append((who, round(t - c["t0"], 3)))
        if "R" in k["flags"]:
            c["rst"].append((who, round(t - c["t0"], 3)))
    for p, c in conns.items():
        ld = round(c["last_data"] - c["t0"], 3) if c["last_data"] else None
        print(
            f"port {p} {c['http']}: dev data segs={c['data']} bytes={c['bytes']} last_data=+{ld}s "
            f"last_dev_seq_end={c['last_seq']} host_last_ack={c['last_ack_from_client']} retx={c['retx']} "
            f"FIN={c['fin']} RST={c['rst']} host_win=[{min(c['cwin'] or [0])},{max(c['cwin'] or [0])}] "
            f"dev_win=[{min(c['dwin'] or [0])},{max(c['dwin'] or [0])}] dev_ctrl_after_last_data={c['after'][:4]}"
        )


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
