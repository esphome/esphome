"""Tabulate run.sh's res.txt files: settle times, opposite DC per channel, DC plus tone, square wave levels.

usage: sum.py RES_TXT...   Judge by the tail values: a segment's min and max include the end of the previous one.
"""

from itertools import zip_longest
import pathlib
import re
import sys


def f(line: str, k: str) -> str:
    return re.search(rf"{k}=(-?[\d.]+)", line).group(1)


def seg(lines: list[str], mode: int, ch: int) -> list[str]:
    return [x for x in lines if x.startswith(f"SEG mode={mode} ") and f" ch{ch} " in x]


for path in sys.argv[1:]:
    lines = pathlib.Path(path).read_text(encoding="utf-8").splitlines()
    st = [line for line in lines if line.startswith("SETTLE")]
    settle = [int(f(line, "settle_1pct")) for line in st]
    tr = [line.split("trace ", 1)[1] for line in st[:1]]
    s2 = [
        (f(a, "tail mean"), f(b, "tail mean") if b else "-")
        for a, b in zip_longest(seg(lines, 2, 0), seg(lines, 2, 1))
    ]
    tone = [(f(line, "tail mean"), f(line, "ac")) for line in seg(lines, 3, 0)]
    squares = {
        mode: [(f(x, "rms"), f(x, "min"), f(x, "max")) for x in seg(lines, mode, 0)]
        for mode in (4, 5)
    }
    print(
        f"== {path}\n settle_1pct ms {settle}\n first trace {tr}\n stereo opposite DC tail mean L,R {s2}\n"
        f" dc+tone tail mean, ac rms {tone}\n fullscale square rms,min,max {squares[4]}\n"
        f" half square rms,min,max {squares[5]}"
    )
