"""jit.py EXT_A EXT_B: compare the on_pronto trigger frames of two extract.py outputs in order; counts identical
frames, frames whose length differs, and the largest difference in Pronto units (timing jitter) between same-length
frames, leaving out the header and the trailing idle."""

import sys

from extract import read_lines, trigger_blocks


def frames(p):
    return [
        [int(x, 16) for x in text.split()]
        for _n, text, _i in trigger_blocks(read_lines(p))
    ]


a, b = frames(sys.argv[1]), frames(sys.argv[2])
same = sum(x == y for x, y in zip(a, b, strict=False))
lens = sum(len(x) != len(y) for x, y in zip(a, b, strict=False))
md = max(
    (
        abs(p - q)
        for x, y in zip(a, b, strict=False)
        if len(x) == len(y)
        for p, q in zip(x[4:-1], y[4:-1], strict=False)
    ),
    default=0,
)
print(
    f"{len(a)}/{len(b)} frames, {same} identical, {lens} length differs, max unit diff {md}"
)
