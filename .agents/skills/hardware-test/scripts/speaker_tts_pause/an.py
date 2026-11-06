"""Per request: first audio, when the intro and the rest were sent, how much played before the rest arrived.

usage: an.py SRV_LOG WWW_DIR LABEL=SERIAL_CAPTURE...
SRV_LOG is srv.py's output with "=== LABEL ..." lines that run.sh writes before each build's requests. The serial
capture needs --ms timestamps and the "[frames" lines from tts-common.yaml. The fifth request of a build is media,
the others announcements. Exits 1 when a build did not get all five requests or a request played nothing.
"""

import datetime as dt
import pathlib
import re
import sys

SRV, WWW = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
RATE = 48000


def ts(line: str) -> float | None:
    m = re.match(r"\[(\d+):(\d+):(\d+)\.(\d+)\]", line)
    return (
        int(m[1]) * 3600 + int(m[2]) * 60 + int(m[3]) + int(m[4]) / 1000 if m else None
    )


def tod(epoch: float) -> float:
    t = dt.datetime.fromtimestamp(epoch)  # noqa: DTZ006  local time, like the serial capture
    return t.hour * 3600 + t.minute * 60 + t.second + t.microsecond / 1e6


sections: dict[str, list[list[str]]] = {}
cur: list[list[str]] | None = None
for line in SRV.read_text(encoding="utf-8").splitlines():
    if line.startswith("=== "):
        cur = sections.setdefault(line.split()[1], [])
    elif line and cur is not None:
        cur.append(line.split(None, 2))
bad = False
for arg in sys.argv[3:]:
    label, serial = arg.split("=", 1)
    sec = sections.get(label, [])
    reqs = [
        (tod(float(e[0])), e[2].split()[0].lstrip("/")) for e in sec if e[1] == "REQ"
    ]
    intro = [tod(float(e[0])) for e in sec if e[1] == "INTRO_SENT"]
    rest = [tod(float(e[0])) for e in sec if e[1] == "REST_SENT"]
    ev = []
    for line in (
        pathlib.Path(serial).read_bytes().decode("utf8", "replace").splitlines()
    ):
        if (t := ts(line)) is None:
            continue
        if "[frames" in line and "reset" in line:
            ev.append((t, "reset", 0))
        elif m := re.search(r"\[frames[^\]]*\]: (media )?played (\d+)", line):
            ev.append((t, "med" if m[1] else "ann", int(m[2])))
    resets = [e[0] for e in ev if e[1] == "reset"]
    print(f"== {label}")
    for i, (rt, name) in enumerate(reqs):
        _, isamp, total = map(
            int, (WWW / f"{name}.cut").read_text(encoding="utf-8").split()
        )
        end = resets[i + 1] if i + 1 < len(resets) else 1e9
        kind, tag = ("media", "med") if i == 4 else ("ann", "ann")
        seg = [e for e in ev if rt - 0.5 < e[0] < end and e[1] == tag]
        if not seg:
            print(f"  {i} {kind} {name}: nothing played")
            bad = True
            continue
        at_rest = max(
            [e[2] for e in seg if i < len(rest) and e[0] <= rest[i]], default=0
        )
        tail = next((round(e[0] - rt, 2) for e in seg if e[2] >= isamp), None)
        sent = (
            f"intro sent {intro[i] - rt:4.2f} s, rest sent {rest[i] - rt:5.2f} s"
            if i < len(rest)
            else "rest not sent"
        )
        print(
            f"  {i} {kind:5} {name}: first audio {seg[0][0] - rt:5.2f} s | {sent} | played before rest "
            f"{at_rest / RATE:5.2f} s of {isamp / RATE:4.2f} s intro | intro fully played at {tail} s | "
            f"final {seg[-1][2] / RATE:5.2f} of {total / RATE:5.2f} s"
        )
    if len(reqs) != 5:
        print(f"  {len(reqs)} requests in the server log, expected 5")
        bad = True
sys.exit(1 if bad else 0)
