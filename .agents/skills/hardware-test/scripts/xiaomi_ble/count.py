"""Count the Xiaomi, ATC and PVVX publishes in a capture and compare them with expected.json from gen.py.
Usage: count.py CAPTURE [pr|base] [--allow-loss N] [--probe]
--probe (injection run): the PR build must leave the probe frame's service data unchanged.
Exits 0 when every count matches the model (or falls short by at most N over the air), 1 otherwise."""

import argparse
import collections
import json
from pathlib import Path
import re
import sys

ap = argparse.ArgumentParser()
ap.add_argument("capture", type=Path)
ap.add_argument("build", nargs="?", choices=["pr", "base"])
ap.add_argument("--allow-loss", type=int, default=0)
ap.add_argument("--probe", action="store_true")
a = ap.parse_args()
cap, build, loss = a.capture, a.build, a.allow_loss
LETTER = {"0A": "A", "0B": "B", "0C": "C", "0D": "D", "0E": "E", "0F": "F", "10": "G"}
GOT = re.compile(
    r"Got (?:Xiaomi \S+|ATC MiThermometer|PVVX MiThermometer) \([AC]4:C1:38:00:00:(\w\w)\)"
)

counts = collections.Counter()
cur = None
humidity = set()
probe = {}
for line in cap.read_text(encoding="utf-8", errors="replace").splitlines():
    if m := GOT.search(line):
        cur = LETTER[m.group(1)]
        continue
    if m := re.search(r"\[probe.*\b(before|after)\s+([0-9A-F]+)", line):
        probe[m.group(1)] = m.group(2)
    if "Humidity:" in line:
        humidity.add(line.split("Humidity:")[1].strip())
    if (m := re.search(r"Temperature: (-?[\d.]+)", line)) and cur:
        t = float(m.group(1))
        tag = cur
        if (
            cur == "A" and t >= 50
        ):  # after-bad frames are 60.1 to 64.0, normal ones 22.1 to 26.0
            tag = "A_after_bad"
        if cur == "A" and abs(t - 33.3) < 0.01:
            tag = "A_probe"  # xb_rewrite_probe's own parse, not part of the model
        if cur == "D" and abs(t - 11.1) < 0.01:
            tag = "D_boot"
        counts[tag] += 1
        cur = None
print(cap.name, dict(sorted(counts.items())))
print("  humidity values", len(humidity))
if not counts:
    print("FAIL: no Xiaomi publishes in the capture")
    sys.exit(1)
if build is None:
    sys.exit(0)
expected = json.loads(Path("expected.json").read_text(encoding="utf-8"))[build]
bad = 0
for tag in sorted((set(expected) | set(counts)) - {"A_probe"}):
    want, got = expected.get(tag, 0), counts.get(tag, 0)
    ok = want - loss <= got <= want
    bad += not ok
    print(f"  {tag:12} want {want:3} got {got:3} {'ok' if ok else 'DIFF'}")
if a.probe:
    before, after = probe.get("before"), probe.get("after")
    print(f"  probe before {before}\n  probe after  {after}")
    if before is None or after is None:
        print("  probe lines missing")
        bad += 1
    elif build == "pr" and before != after:
        print("  the listener rewrote the shared service data")
        bad += 1
print(
    f"{'PASS' if not bad else 'FAIL'}: {build} model {'matches' if not bad else f'has {bad} failed checks'}"
)
sys.exit(1 if bad else 0)
