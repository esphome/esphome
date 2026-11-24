"""hostorder.py HOSTLOG [MATCH]: one line per USB enumeration in a hostlog.sh capture.

Prints the time from "disable LowDataMode" to setupDataTransfers (the host selecting alt 1), ABORT when the NCM IN
pipe (endpoint 0x83) was aborted while the device stayed attached (the host went back to alt 0), and LINK1 or NO-LINK
for the connection notification (updateLinkStatus: linkStatus 1). MATCH (default W5100S) is in the USB product string.
Assumes one NCM adapter on the host: other adapters' transfer and link messages are not told apart from the target's.
"""

import pathlib
import re
import sys


def ms(t: str) -> float:
    h, m, s = t.split(":")
    return (int(h) * 3600 + int(m) * 60 + float(s)) * 1000


def main(path: str, match: str) -> None:
    rows: list[dict] = []
    cur: dict | None = None
    with pathlib.Path(path).open(errors="replace", encoding="utf-8") as f:
        for line in f:
            p = line.split()
            if len(p) < 3 or not re.match(r"\d\d:\d\d:\d\d\.\d+", p[1]):
                continue
            t = ms(p[1])
            if "enumerateDeviceComplete" in line and match in line:
                cur = {"enum": p[1]}
                rows.append(cur)
            elif cur is None:
                continue
            elif "disable LowDataMode on en" in line:
                cur.setdefault("low", t)
            elif "setupDataTransfers" in line:
                cur.setdefault("alt1", t)
            elif "linkStatus 1" in line:
                cur.setdefault("link1", t)
            elif (
                "endpoint 0x83: aborting" in line
                and "terminate" not in line
                and "alt1" in cur
                and "link1" not in cur
                and "term" not in cur
            ):
                cur.setdefault("abort", t)
            elif "terminateDevice" in line and match in line:
                cur.setdefault("term", t)
    for r in rows:
        if "alt1" not in r:
            continue
        low = (
            f"low->alt1 {r['alt1'] - r['low']:6.0f} ms"
            if "low" in r
            else "low->alt1      -"
        )
        abort = f"ABORT +{r['abort'] - r['alt1']:.0f} ms" if "abort" in r else ""
        print(r["enum"], low, abort, "LINK1" if "link1" in r else "NO-LINK")


main(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "W5100S")
