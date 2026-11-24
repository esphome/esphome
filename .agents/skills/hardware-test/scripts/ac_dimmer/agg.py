"""Aggregate ac_dimmer harness captures per phase (clean, noise), levels below 100 % only.

usage: agg.py CAPTURE [CAPTURE ...]
Per file and phase: gate rises checked, rises and falls more than 100 us off, the start error range and mean, the
longest gate, and the summed missed and doubled half cycles from the WIN lines (lower bounds if lines were lost).
"""

import pathlib
import re
import sys

WIN_RE = re.compile(r"(WIN|ERR) (sweep|hold) phase=(\w+) level=(\d+) (.*)")
ERR_RE = re.compile(
    r"rise n=(\d+) off=(\d+) min=(-?\d+) max=(-?\d+) mean=(-?[\d.]+) sd=([\d.]+) \| fall n=(\d+) off=(\d+)"
    r".*width min=(\d+) max=(\d+)"
)


def agg(f: str) -> dict:
    with pathlib.Path(f).open("rb") as fh:
        t = fh.read().decode("latin1")
    out = {}
    hold = 0
    for m in WIN_RE.finditer(t):
        kind, tag, ph, lv, rest = m.groups()
        if lv == "100":
            continue
        if tag == "hold":
            if kind == "WIN":
                hold += 1
            if hold <= 1:
                continue
            ph = "clean"
        o = out.setdefault(
            ph,
            {
                "win": 0,
                "missed": 0,
                "dbl": 0,
                "n": 0,
                "roff": 0,
                "foff": 0,
                "rmin": 9e9,
                "rmax": -9e9,
                "wmax": 0,
                "errsum": 0.0,
            },
        )
        if kind == "WIN":
            d = dict(re.findall(r"(\w+)=(-?\d+)", rest))
            o["win"] += 1
            o["missed"] += int(d["missed"])
            o["dbl"] += int(d["dbl"])
        elif g := ERR_RE.search(rest):
            g = g.groups()
            o["n"] += int(g[0])
            o["roff"] += int(g[1])
            o["foff"] += int(g[7])
            o["rmin"] = min(o["rmin"], int(g[2]))
            o["rmax"] = max(o["rmax"], int(g[3]))
            o["wmax"] = max(o["wmax"], int(g[9]))
            o["errsum"] += float(g[4]) * int(g[0])
    return out


for f in sys.argv[1:]:
    for ph, o in agg(f).items():
        print(
            f"{f:24} {ph:5} rises={o['n']} rise_off={o['roff']} fall_off={o['foff']} err {o['rmin']}..{o['rmax']}"
            f" mean {o['errsum'] / max(o['n'], 1):.1f} wmax={o['wmax']} | WINlines={o['win']}"
            f" missed={o['missed']} dbl={o['dbl']}"
        )
