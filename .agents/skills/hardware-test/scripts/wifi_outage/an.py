"""Per outage summary for outage.py runs.

usage: an.py LABEL [LABEL...]   (reads LABEL-marks.txt, LABEL-dut.txt, LABEL-rtr.txt)

Per outage (from RESTARTING_SEEN to the next DOWN or END):
  skip     the DUT entered RESTARTING with the fallback AP up and the restart was skipped (no "Restarting adapter")
  cd_s     seconds from that RESTARTING transition to the first state machine line after the cooldown
  assoc    the driver associated on its own during the cooldown (no "Connecting to" before the association)
  ip_cd    the address arrived before the cooldown ended
  bug      "Connected" followed by "Connection lost; reconnecting" within 5 s
  held     the first "Connected" was not followed by a loss within 60 s
  rel      DHCPRELEASE sent while the router was up (after UP)
  rleave   router saw the DUT leave after UP
"""

from datetime import datetime
import pathlib
import re
import sys


def t(s: str) -> float:
    d = datetime.strptime(s, "%H:%M:%S.%f")  # noqa: DTZ007
    return d.hour * 3600 + d.minute * 60 + d.second + d.microsecond / 1e6


def load(path: str) -> list[tuple[float, str]]:
    out = []
    with pathlib.Path(path).open(encoding="utf-8", errors="replace") as f:
        out.extend(
            (t(line[:12]), line[13:].rstrip("\n"))
            for line in f
            if len(line) > 13 and line[2] == ":" and line[12] == " "
        )
    return out


CONNECTED = re.compile(r"\[I\]\[wifi:\d+\]: Connected$")
LOST = "Connection lost; reconnecting"


def analyze(label: str) -> list[dict]:
    marks = load(f"{label}-marks.txt")
    dut = load(f"{label}-dut.txt")
    rtr = load(f"{label}-rtr.txt")
    rows = []
    downs = [(ts, m) for ts, m in marks if " DOWN" in m or m.startswith("MARK END")]
    for ts, m in marks:
        g = re.match(r"MARK OUTAGE (\d+) RESTARTING_SEEN", m)
        if not g:
            continue
        n = int(g.group(1))
        up = next((u for u, mm in marks if mm == f"MARK OUTAGE {n} UP"), ts)
        end = next((d for d, _ in downs if d > ts), dut[-1][0] + 1)
        win = [(a, b) for a, b in dut if ts - 3 <= a < end]
        # the RESTARTING transition the harness reacted to
        rst = max(
            (a for a, b in win if "→ RESTARTING" in b and a <= ts + 0.5), default=ts
        )
        after = [(a, b) for a, b in win if a > rst]
        skip = not any("Restarting adapter" in b for a, b in after if a - rst < 1.0)
        # first state machine line after the cooldown
        cd_end = next(
            (
                a
                for a, b in after
                if a - rst > 1.0
                and (
                    CONNECTED.search(b)
                    or "Retry phase" in b
                    or "Connecting to" in b
                    or "Unknown connection" in b
                    or "failed (callback)" in b
                )
            ),
            None,
        )
        first_connecting = next((a for a, b in after if "Connecting to" in b), 1e12)
        assoc_t = next(
            (a for a, b in after if "wifi_lt" in b and "Connected ssid=" in b), None
        )
        assoc = (
            assoc_t is not None
            and assoc_t < first_connecting
            and (cd_end is None or assoc_t < cd_end)
        )
        ip_t = next((a for a, b in after if "static_ip=" in b), None)
        ip_cd = ip_t is not None and cd_end is not None and ip_t < cd_end
        conns = [a for a, b in after if CONNECTED.search(b)]
        losts = [a for a, b in after if LOST in b]
        bug = sum(1 for c in conns if any(0 <= lost - c <= 5 for lost in losts))
        held = bool(conns) and not any(0 <= lost - conns[0] <= 60 for lost in losts)
        rel = sum(1 for a, b in win if a >= up and "DHCPRELEASE sent" in b)
        rleave = sum(
            1 for a, b in rtr if up <= a < end and "AP client disconnected" in b
        )
        rows.append(
            {
                "label": label,
                "n": n,
                "skip": skip,
                "cd_s": round(cd_end - rst, 1) if cd_end else None,
                "up_s": round(up - rst, 1),
                "assoc": assoc,
                "ip_cd": ip_cd,
                "bug": bug,
                "held": held,
                "rel": rel,
                "rleave": rleave,
            }
        )
    return rows


def main() -> None:
    allrows = []
    for label in sys.argv[1:]:
        allrows += analyze(label)
    keys = [
        "label",
        "n",
        "skip",
        "up_s",
        "cd_s",
        "assoc",
        "ip_cd",
        "bug",
        "held",
        "rel",
        "rleave",
    ]
    print("\t".join(keys))
    for r in allrows:
        print("\t".join(str(r[k]) for k in keys))
    print(
        f"TOTAL outages={len(allrows)} skip={sum(r['skip'] for r in allrows)} "
        f"returned_in_cooldown={sum(1 for r in allrows if r['skip'] and r['cd_s'] and r['up_s'] < r['cd_s'])} "
        f"assoc_in_cooldown={sum(r['assoc'] for r in allrows)} ip_in_cooldown={sum(r['ip_cd'] for r in allrows)} "
        f"bug={sum(r['bug'] for r in allrows)} held={sum(r['held'] for r in allrows)} "
        f"rel={sum(r['rel'] for r in allrows)} rleave={sum(r['rleave'] for r in allrows)}"
    )


if __name__ == "__main__":
    main()
