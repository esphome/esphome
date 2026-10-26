"""Summarise the client's results per upstream phase and the DUT's connects, AP starts, crashes and boots.
Usage: sum.py DIR NAME   reads NAME-marks.txt, NAME-cli.txt and NAME-dut.txt
Phase 0 is before the first mark. Exits 1 when the DUT crashed or rebooted, or the client logged nothing."""

from collections import defaultdict
from pathlib import Path
import re
import sys

D, n = Path(sys.argv[1]), sys.argv[2]


def read(suffix: str) -> str:
    return (D / f"{n}-{suffix}.txt").read_text(encoding="utf-8", errors="replace")


marks = [line.split()[0] for line in read("marks").splitlines() if line.strip()]
cli = [line for line in read("cli").splitlines() if "[cli" in line]


def phase(t: str) -> int:
    return sum(1 for m in marks if t >= m)


stat: dict[int, dict[str, int]] = defaultdict(lambda: defaultdict(int))
for line in cli:
    m = re.search(r"\]: (NOT CONNECTED|NET .*|NAME \S+|GW \S+|MAC \S+)", line)
    if not m:
        continue
    s = m.group(1)
    if s.startswith("NET"):
        dns = re.search(r"dns (\S+)", s).group(1)
        s = f"dns={dns} resolve={s.split('-> ')[1].strip()}"
    stat[phase(line[1:9])][s] += 1
for p in sorted(stat):
    print(f"phase {p}:", dict(stat[p]))
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
ok = cli and not crash and boots <= 1
print(
    f"{'PASS' if ok else 'FAIL'}: {'no crash or reboot' if ok else 'crash, reboot or an empty client log'}"
)
sys.exit(0 if ok else 1)
