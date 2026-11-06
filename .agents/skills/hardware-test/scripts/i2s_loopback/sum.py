"""Tabulate run.sh's res.txt files: settle times, opposite DC per channel, DC plus tone, square wave levels.

usage: sum.py RES_TXT...   Judge by the tail values: a segment's min and max include the end of the previous one.
"""

import pathlib
import re
import sys


def f(line: str, k: str) -> str:
    return re.search(rf"{k}=(-?[\d.]+)", line).group(1)


for path in sys.argv[1:]:
    lines = pathlib.Path(path).read_text(encoding="utf-8").splitlines()
    st = [line for line in lines if line.startswith("SETTLE")]
    settle = [int(f(line, "settle_1pct")) for line in st]
    tr = [re.search(r"trace (.*)", line).group(1) for line in st[:1]]

    def seg(mode: int, ch: int, lines: list[str] = lines) -> list[str]:
        return [
            line
            for line in lines
            if line.startswith(f"SEG mode={mode} ") and f" ch{ch} " in line
        ]

    if seg(2, 1):
        s2 = [
            (f(a, "tail mean"), f(b, "tail mean"))
            for a, b in zip(seg(2, 0), seg(2, 1), strict=False)
        ]
    else:
        s2 = [(f(a, "tail mean"), "-") for a in seg(2, 0)]
    tone = [(f(line, "tail mean"), f(line, "ac")) for line in seg(3, 0)]
    sq = [(f(line, "rms"), f(line, "min"), f(line, "max")) for line in seg(4, 0)]
    hs = [(f(line, "rms"), f(line, "min"), f(line, "max")) for line in seg(5, 0)]
    print(
        f"== {path}\n settle_1pct ms {settle}\n first trace {tr}\n stereo opposite DC tail mean L,R {s2}\n"
        f" dc+tone tail mean, ac rms {tone}\n fullscale square rms,min,max {sq}\n half square rms,min,max {hs}"
    )
