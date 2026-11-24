"""Print the per window lines of ac_dimmer harness captures.

usage: ana.py CAPTURE [CAPTURE ...]
Boots and crashes per file, then for each sweep level (clean and noise) and each hold report after the first: zero
crossings, injected glitches, gate rises, missed and doubled half cycles, and the start and end error of the gates
against the expected times (microseconds).
"""

import pathlib
import re
import sys

WIN_RE = re.compile(r"(WIN|ERR) (sweep|hold) phase=(\w+) level=(\d+) (.*)")
ERR_RE = re.compile(
    r"rise n=(\d+) off=(\d+) min=(-?\d+) max=(-?\d+) mean=(-?[\d.]+) sd=([\d.]+) \| fall n=(\d+) off=(\d+)"
    r" min=(-?\d+) max=(-?\d+) mean=(-?[\d.]+) sd=([\d.]+) \| width min=(\d+) max=(\d+)"
)

for f in sys.argv[1:]:
    with pathlib.Path(f).open("rb") as fh:
        t = fh.read().decode("latin1")
    crashes = len(re.findall(r"Exception|rst cause|Soft WDT|wdt reset", t))
    print("==", f, "boots", t.count("BOOT reset"), "exc", crashes)
    hold = 0
    for m in WIN_RE.finditer(t):
        kind, tag, ph, lv, rest = m.groups()
        if tag == "hold":
            if kind == "WIN":
                hold += 1
            if hold <= 1:
                continue
        if kind == "WIN":
            d = dict(re.findall(r"(\w+)=(-?\d+)", rest))
            print(
                f"{ph:5} {lv:>3} WIN zc={d['zc']} glitch={d['glitch']} rises={d['rises']} missed={d['missed']}"
                f" dbl={d['dbl']} hi={d['hi_at_zc']}"
            )
        elif r := ERR_RE.search(rest):
            g = r.groups()
            print(
                f"{ph:5} {lv:>3} ERR rise n={g[0]} off={g[1]} err {g[2]}..{g[3]} mean {g[4]} sd {g[5]}"
                f" | fall off={g[7]} err {g[8]}..{g[9]} | w {g[12]}..{g[13]}"
            )
