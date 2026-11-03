"""Summarise the AP restart run: alive lines over syslog per DUT, reboots, gaps, recovery after each AP start.
Usage: an.py DIR BUILD [RECOVER_S]   reads BUILD-marks.txt, BUILD-syslog.txt, BUILD-bk.txt, BUILD-cb.txt (optional)
and BUILD-up.txt (the bridge's API logs)
Exits 1 when a DUT rebooted, crashed, sent nothing, or had no syslog line within RECOVER_S (default 120) of an
AP start, when the BK7238 capture (or a CB3S capture that was started) is empty, or when the bridge did not switch
its AP to WPA2/WPA3 mixed after each AP start."""

from itertools import pairwise
from pathlib import Path
import re
import sys

D, b = Path(sys.argv[1]), sys.argv[2]
RECOVER = int(sys.argv[3]) if len(sys.argv) > 3 else 120


def read(suffix: str) -> str | None:
    f = D / f"{b}-{suffix}.txt"
    return f.read_text(encoding="utf-8", errors="replace") if f.exists() else None


def secs(t: str) -> int:
    h, m, s = t.split(":")
    return int(h) * 3600 + int(m) * 60 + int(float(s))


marks = [
    (secs(line.split()[0]), line.split()[3])
    for line in read("marks").splitlines()
    if line.strip()
]
sl = (read("syslog") or "").splitlines()
bad = []
for dev in ("rebk", "recb"):
    lines = [line for line in sl if re.search(rf"\b{dev}\b", line)]
    ups = [
        (secs(line.split()[0]), int(m[1]))
        for line in lines
        if (m := re.search(r"up (\d+) s", line))
    ]
    if not ups:
        print(f"== {dev}: no alive lines over syslog")
        bad.append(f"{dev} silent")
        continue
    reboots = sum(1 for (_, u0), (_, u1) in pairwise(ups) if u1 < u0)
    gaps = [t1 - t0 for (t0, _), (t1, _) in pairwise(ups) if t1 - t0 > 25]
    print(
        f"== {dev}: alive lines {len(ups)}, reboots {reboots}, syslog gaps over 25 s: {gaps}"
    )
    if reboots:
        bad.append(f"{dev} rebooted")
    for t, st in marks:
        if st != "on":
            continue
        after = [x for x in ups if x[0] > t]
        first = after[0][0] - t if after else None
        print(
            f"   ap back at t={t % 86400 // 3600:02d}:{t % 3600 // 60:02d}:{t % 60:02d}, first syslog line {first} s later"
        )
        if first is None or first > RECOVER:
            bad.append(f"{dev} not back {RECOVER} s after an AP start")
for dev, suffix in (("rebk", "bk"), ("recb", "cb")):
    if (s := read(suffix)) is None and dev == "recb":
        continue  # the CB3S capture is optional
    if not s:
        bad.append(f"{dev} serial capture missing or empty")
        continue
    crashes = len(re.findall(r"(?i)hard fault|exception|abort|watchdog", s))
    connected = len(re.findall(r"\[wifi:\d+\]: Connected", s))
    print(
        f"-- {dev} serial: lines {s.count(chr(10))}, overflow {s.count('queue overflow')}, fallback AP starts "
        f"{s.count('Starting fallback AP')}, connected {connected}, boots {s.count('ESPHome version')}, crashes {crashes}"
    )
    if crashes:
        bad.append(f"{dev} crashed")
up = read("up") or ""
switches = [
    secs(m[1]) for m in re.finditer(r"\[(\d+:\d+:\d+)[.\d]*\].*PMF capable: 0", up)
]
# each AP start needs its own switch between that mark (less a few seconds of clock skew) and the next mark
ends = [t for t, _ in marks[1:]] + [float("inf")]
missed = [
    t
    for (t, st), end in zip(marks, ends, strict=True)
    if st == "on" and not any(t - 5 <= s < end for s in switches)
]
starts = sum(1 for _, st in marks if st == "on")
print(
    f"-- bridge: WPA2/WPA3 switches {len(switches)}, AP starts {starts} ({len(missed)} without a switch), "
    f"failures {up.count('AP CONFIG FAILED')}"
)
if "AP CONFIG FAILED" in up or missed:
    bad.append("the bridge did not switch its AP to WPA2/WPA3 mixed after every start")
print("PASS" if not bad else "FAIL: " + ", ".join(bad))
sys.exit(1 if bad else 0)
