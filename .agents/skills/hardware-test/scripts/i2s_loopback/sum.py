"""Tabulate run.sh's res.txt files: settle times, opposite DC per channel, DC plus tone, square wave levels.

usage: sum.py [--stereo] RES_TXT...   Judge by the tail values: a segment's min and max include the end of the
previous one. A file is checked as stereo with --stereo or when it has any channel 1 line.
Exits 1 when a file lacks the three segments of each signal mode on channel 0 (and channel 1 when stereo), or the
full-scale square never reaches an rms of 8000 on one of those channels (no loopback signal: a missing jumper or a
dead generator).
"""

import pathlib
import re
import sys


def f(line: str, k: str) -> str:
    return re.search(rf"{k}=(-?[\d.]+)", line).group(1)


def seg(lines: list[str], mode: int, ch: int) -> list[str]:
    return [x for x in lines if x.startswith(f"SEG mode={mode} ") and f" ch{ch} " in x]


FORCE_STEREO = "--stereo" in sys.argv[1:]
bad = False
for path in [a for a in sys.argv[1:] if a != "--stereo"]:
    lines = pathlib.Path(path).read_text(encoding="utf-8").splitlines()
    st = [line for line in lines if line.startswith("SETTLE")]
    settle = [int(f(line, "settle_1pct")) for line in st]
    tr = [line.split("trace ", 1)[1] for line in st[:1]]
    stereo = FORCE_STEREO or any(" ch1 " in x for x in lines if x.startswith("SEG "))
    chans = (0, 1) if stereo else (0,)
    s2 = [
        tuple(f(x, "tail mean") for x in pair)
        for pair in zip(*(seg(lines, 2, c) for c in chans), strict=False)
    ]
    tone = [(f(line, "tail mean"), f(line, "ac")) for line in seg(lines, 3, 0)]
    squares = {
        (mode, c): [
            (f(x, "rms"), f(x, "min"), f(x, "max")) for x in seg(lines, mode, c)
        ]
        for mode in (4, 5)
        for c in chans
    }
    print(
        f"== {path}\n settle_1pct ms {settle}\n first trace {tr}\n opposite DC tail mean per channel {s2}\n"
        f" dc+tone tail mean, ac rms {tone}"
    )
    for c in chans:
        print(
            f" ch{c} fullscale square rms,min,max {squares[4, c]}\n"
            f" ch{c} half square rms,min,max {squares[5, c]}"
        )
    for c in chans:
        if short := [m for m in range(1, 6) if len(seg(lines, m, c)) < 3]:
            print(f" FAIL: fewer than 3 ch{c} segments for modes {short}")
            bad = True
        if max((float(rms) for rms, _, _ in squares[4, c]), default=0) < 8000:
            print(
                f" FAIL: the full-scale square never reached rms 8000 on ch{c}, no loopback signal"
            )
            bad = True
sys.exit(1 if bad else 0)
