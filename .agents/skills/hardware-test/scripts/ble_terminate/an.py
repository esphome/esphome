"""Match each proxy disconnect with the peer's own disconnect line and count the links the peer kept.
Usage: an.py DIR LABEL [ROUNDS]   reads LABEL-drive.txt, LABEL-peer.txt (serial_capture --ms) and LABEL-proxy.txt
(esphome logs, VERBOSE). A round counts only when the last `Conn params updated` the proxy logged since the previous
disconnect shows latency=10; otherwise it is inconclusive. Exits 1 when the peer kept a link past 3 s (the terminate
was lost), a round was inconclusive, or fewer than ROUNDS rounds completed."""

import datetime as dt
from pathlib import Path
import re
import sys

D, label = Path(sys.argv[1]), sys.argv[2]
ROUNDS = int(sys.argv[3]) if len(sys.argv) > 3 else None


def read(name: str) -> list[str]:
    return (
        (D / f"{label}-{name}.txt")
        .read_text(encoding="utf-8", errors="replace")
        .splitlines()
    )


def ptime(line: str) -> float | None:
    """Seconds since midnight from a leading [HH:MM:SS.mmm] stamp."""
    m = re.match(r"\[(\d+):(\d+):(\d+)\.(\d+)\]", line)
    return (
        int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000 if m else None
    )


pdisc = [
    t
    for line in read("peer")
    if "PEER DISCONNECT" in line and (t := ptime(line)) is not None
]
params = [
    (t, int(m[1]))
    for line in read("proxy")
    if "Conn params updated" in line
    and (m := re.search(r"latency=(\d+)", line))
    and (t := ptime(line)) is not None
]
res = []
prev = -1.0
for line in read("drive"):
    m = re.search(
        r"R(\d+) .*DISC_CALL ([\d.]+) DISC_DONE ([\d.]+) \(\+([\d.]+)s\)", line
    )
    if not m:
        if line.startswith("R"):
            print("  ", line)
        continue
    # Both captures stamp local wall clock time; compare as seconds since midnight.
    t = dt.datetime.fromtimestamp(float(m[2])).astimezone()
    tc = t.hour * 3600 + t.minute * 60 + t.second + t.microsecond / 1e6
    after = [p - tc for p in pdisc if -1 < p - tc < 12]
    window = [lat for pt, lat in params if prev < pt <= tc]
    res.append(
        (
            int(m[1]),
            float(m[4]),
            after[0] if after else None,
            window[-1] if window else None,
        )
    )
    prev = tc
for r, done, pd, lat in res:
    peer = (
        f"peer disconnect +{pd:.2f}s" if pd is not None else "peer disconnect not seen"
    )
    print(f"  R{r:2} central done +{done:.3f}s  {peer}  latency at disconnect {lat}")
late = sum(1 for _, _, pd, lat in res if lat == 10 and (pd is None or pd > 3))
inconclusive = sum(1 for *_, lat in res if lat != 10)
print(
    f"{label}: rounds {len(res)}, peer kept the link (>3 s or not seen) {late}, inconclusive {inconclusive}"
)
failed = []
if not res:
    failed.append("no round completed")
if ROUNDS is not None and len(res) < ROUNDS:
    failed.append(f"only {len(res)} of {ROUNDS} rounds completed")
if late:
    failed.append(f"{late} links kept")
if inconclusive:
    failed.append(
        f"INCONCLUSIVE: {inconclusive} rounds had no latency=10 update before the disconnect"
    )
print(f"PASS: {label}" if not failed else f"FAIL: {label}: " + "; ".join(failed))
sys.exit(1 if failed else 0)
