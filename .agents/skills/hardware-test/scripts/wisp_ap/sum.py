"""Summarise the client's results per upstream phase and the DUT's connects, AP starts, crashes and boots.
Usage: sum.py DIR NAME   reads NAME-marks.txt, NAME-cli.txt and NAME-dut.txt
Phase N runs from mark N (the first mark is the start state). Exits 1 when the DUT crashed, the DUT capture does not
hold exactly one boot (empty or rebooted), the client had no successful result at all, or a phase with the upstream
on had no successful name lookup."""

from collections import defaultdict
from pathlib import Path
import re
import sys

D, n = Path(sys.argv[1]), sys.argv[2]


def read(suffix: str) -> str:
    return (D / f"{n}-{suffix}.txt").read_text(encoding="utf-8", errors="replace")


mark_lines = [line.split() for line in read("marks").splitlines() if line.strip()]
marks = [m[0] for m in mark_lines]
upstream = {
    i + 1: m[3] for i, m in enumerate(mark_lines)
}  # "HH:MM:SS t=N upstream on|off CODE"
cli = [line for line in read("cli").splitlines() if "[cli" in line]


def phase(t: str) -> int:
    return sum(1 for m in marks if t >= m)


stat: dict[int, dict[str, int]] = defaultdict(lambda: defaultdict(int))
good: dict[int, int] = defaultdict(int)  # successful name lookups per phase
any_ok = 0
for line in cli:
    m = re.search(r"\]: (NOT CONNECTED|NET .*|NAME \S+|GW \S+|MAC \S+)", line)
    if not m:
        continue
    s = m.group(1)
    if s.startswith("NET"):
        dns = re.search(r"dns (\S+)", s).group(1)
        res = s.split("-> ")[1].strip()
        s = f"dns={dns} resolve={res}"
        if res != "FAIL":
            good[phase(line[1:9])] += 1
            any_ok += 1
    elif re.match(r"(NAME|GW|MAC) 200\b", s):
        any_ok += 1
    stat[phase(line[1:9])][s] += 1
for p in sorted(stat):
    print(f"phase {p} (upstream {upstream.get(p, '?')}):", dict(stat[p]))
dead = [p for p, st in upstream.items() if st == "on" and not good[p]]
dut = read("dut")
crash = len(re.findall(r"Guru|panic|abort\(", dut))
boots = dut.count("ESPHome version")
print(
    "DUT connected:",
    len(re.findall(r"\[wifi:\d+\]: Connected", dut)),
    "AP starts:",
    dut.count("Starting AP"),
    "crash:",
    crash,
    "boots:",
    boots,
)
failed = []
if not cli:
    failed.append("empty client log")
elif not any_ok:
    failed.append("the client never got a successful result")
if dead:
    failed.append(f"no successful lookup in upstream on phases {dead}")
# run.sh resets the DUT when the capture starts, so a good capture holds exactly one boot
if boots != 1:
    failed.append(f"{boots} DUT boots captured, expected 1")
if crash:
    failed.append(f"{crash} DUT crash lines")
print("PASS" if not failed else "FAIL: " + "; ".join(failed))
sys.exit(1 if failed else 0)
