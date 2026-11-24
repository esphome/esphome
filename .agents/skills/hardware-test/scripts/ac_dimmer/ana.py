"""Summarise ac_dimmer harness captures.

usage: ana.py [--sum] CAPTURE [CAPTURE ...]
Default: boots and crashes per file, then for each sweep level (clean and noise) and each hold report after the first:
zero crossings, injected glitches, gate rises, missed and doubled half cycles, and the start and end error of the gates
against the expected times (microseconds).
--sum: per file and phase (hold counts as clean), levels below 100 % only: gate rises checked, rises and falls more
than 100 us off, the start error range and mean, the longest gate, and the summed missed and doubled half cycles
(lower bounds if WIN lines were lost).
"""

from collections import defaultdict
import pathlib
import re
import sys

LINE_RE = re.compile(r"(WIN|ERR) (sweep|hold) phase=(\w+) level=(\d+) (.*)")
KV_RE = re.compile(r"(\w+)=(-?[\d.]+)")


def windows(text: str):
    """Yield (kind, phase, level, fields) per WIN/ERR line, skipping the first hold report."""
    hold = 0
    for m in LINE_RE.finditer(text):
        kind, tag, ph, lv, rest = m.groups()
        if tag == "hold":
            hold += kind == "WIN"
            if hold <= 1:
                continue
        yield kind, ph, lv, {k: float(v) for k, v in KV_RE.findall(rest)}


def summed(text: str) -> dict:
    out = defaultdict(lambda: defaultdict(float, rmin=9e9, rmax=-9e9))
    for kind, ph, lv, d in windows(text):
        if lv == "100":
            continue
        o = out["clean" if ph == "hold" else ph]
        if kind == "WIN":
            o["win"] += 1
            o["missed"] += d["missed"]
            o["dbl"] += d["dbl"]
        else:
            o["n"] += d["rn"]
            o["roff"] += d["roff"]
            o["foff"] += d["foff"]
            o["rmin"] = min(o["rmin"], d["rmin"])
            o["rmax"] = max(o["rmax"], d["rmax"])
            o["wmax"] = max(o["wmax"], d["wmax"])
            o["errsum"] += d["rmean"] * d["rn"]
    return out


args = sys.argv[1:]
total = args[:1] == ["--sum"]
for f in args[total:]:
    with pathlib.Path(f).open("rb") as fh:
        t = fh.read().decode("latin1")
    if total:
        for ph, o in summed(t).items():
            print(
                f"{f:24} {ph:5} rises={o['n']:.0f} rise_off={o['roff']:.0f} fall_off={o['foff']:.0f}"
                f" err {o['rmin']:.0f}..{o['rmax']:.0f} mean {o['errsum'] / max(o['n'], 1):.1f} wmax={o['wmax']:.0f}"
                f" | WINlines={o['win']:.0f} missed={o['missed']:.0f} dbl={o['dbl']:.0f}"
            )
        continue
    crashes = len(re.findall(r"Exception|rst cause|Soft WDT|wdt reset", t))
    print("==", f, "boots", t.count("BOOT reset"), "exc", crashes)
    for kind, ph, lv, d in windows(t):
        g = {k: f"{v:g}" for k, v in d.items()}
        if kind == "WIN":
            print(
                f"{ph:5} {lv:>3} WIN zc={g['zc']} glitch={g['glitch']} rises={g['rises']} missed={g['missed']}"
                f" dbl={g['dbl']} hi={g['hi_at_zc']}"
            )
        else:
            print(
                f"{ph:5} {lv:>3} ERR rise n={g['rn']} off={g['roff']} err {g['rmin']}..{g['rmax']} mean {g['rmean']}"
                f" sd {g['rsd']} | fall off={g['foff']} err {g['fmin']}..{g['fmax']} | w {g['wmin']}..{g['wmax']}"
            )
