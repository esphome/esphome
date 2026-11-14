"""stallctr.py PCAP DEV_IP APILOG: for each /events connection print last data and FIN wall times, and the atcp counter
line just before the last data, just after it (about +1.5 s), and about 7 s later (after the FIN). APILOG comes from
apilog.py with counter log lines tagged [atcp."""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from pcapsum import packets  # noqa: E402


def main(pcap: str, dev: str, logf: str) -> None:
    conns: dict[str, dict] = {}
    for k in packets(pcap, dev):
        c = conns.setdefault(
            k["port"], {"t0": k["t"], "ev": False, "last": None, "fin": None}
        )
        if "GET /events" in k["line"]:
            c["ev"] = True
        if k["from_dev"] and k["len"] > 0:
            c["last"] = k["t"]
        if k["from_dev"] and "F" in k["flags"] and c["fin"] is None:
            c["fin"] = k["t"]
    logs = []
    for line in pathlib.Path(logf).read_text(encoding="utf-8").splitlines():
        if "[atcp" in line:
            t, rest = line.split(" ", 1)
            logs.append((float(t), rest.strip().split("]: ", 1)[-1]))

    def at(t: float) -> tuple[float, str] | None:
        best = None
        for lt, s in logs:
            if lt <= t:
                best = (lt, s)
        return best

    events = sum(1 for c in conns.values() if c["ev"] and c["last"] is not None)
    print(f"{events} /events connections with data, {len(logs)} atcp counter lines")
    if not events or not logs:
        sys.exit(
            "FAIL: nothing to line up (no /events connection in the pcap or no [atcp line in the API log)"
        )
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
            if a := at(t):
                print(f"   {lbl:7} {a[1]}")


if __name__ == "__main__":
    main(*sys.argv[1:4])
