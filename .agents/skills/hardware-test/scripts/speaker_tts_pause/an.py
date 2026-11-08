"""Per request: first audio, when the intro and the rest were sent, how much played before the rest arrived.

usage: an.py SRV_LOG WWW_DIR LABEL=SERIAL_CAPTURE...
SRV_LOG is srv.py's output with "=== LABEL ..." lines that run.sh writes before each build's requests. The serial
capture needs --ms timestamps and the "[frames" lines from tts-common.yaml. The fifth request of a build is media,
the others announcements. Exits 1 when a build did not get all five requests, a request played nothing, never played
the whole intro or stopped short of 95 % of the stream, or the server logged an error or never sent the rest of a
request.
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


# per build label, one dict per REQ with the send events that follow it (requests are sequential)
sections: dict[str, list[dict]] = {}
cur: list[dict] | None = None
for line in SRV.read_text(encoding="utf-8").splitlines():
    if line.startswith("=== "):
        cur = sections.setdefault(line.split()[1], [])
        continue
    e = line.split(None, 2)
    if cur is None or len(e) < 2 or not re.fullmatch(r"\d+\.\d+", e[0]):
        continue  # tracebacks and other stray output
    if e[1] == "REQ" and len(e) == 3:
        cur.append({"t": tod(float(e[0])), "name": e[2].split()[0].lstrip("/")})
    elif e[1] in ("INTRO_SENT", "REST_SENT", "ERR") and cur:
        cur[-1].setdefault(e[1], tod(float(e[0])))
bad = False
for arg in sys.argv[3:]:
    label, serial = arg.split("=", 1)
    sec = sections.get(label, [])
    reqs = [(r["t"], r["name"]) for r in sec]
    intro = [r.get("INTRO_SENT") for r in sec]
    rest = [r.get("REST_SENT") for r in sec]
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
        at_rest = max([e[2] for e in seg if rest[i] and e[0] <= rest[i]], default=0)
        tail = next((round(e[0] - rt, 2) for e in seg if e[2] >= isamp), None)
        if rest[i] and intro[i]:
            sent = f"intro sent {intro[i] - rt:4.2f} s, rest sent {rest[i] - rt:5.2f} s"
        else:
            sent = "rest not sent" + (" (server ERR)" if "ERR" in sec[i] else "")
            bad = True
        if tail is None or seg[-1][2] < 0.95 * total:
            bad = True
        print(
            f"  {i} {kind:5} {name}: first audio {seg[0][0] - rt:5.2f} s | {sent} | played before rest "
            f"{at_rest / RATE:5.2f} s of {isamp / RATE:4.2f} s intro | intro fully played at {tail} s | "
            f"final {seg[-1][2] / RATE:5.2f} of {total / RATE:5.2f} s"
        )
    if len(reqs) != 5:
        print(f"  {len(reqs)} requests in the server log, expected 5")
        bad = True
sys.exit(1 if bad else 0)
