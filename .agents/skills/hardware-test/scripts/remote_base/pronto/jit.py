import pathlib
import sys


def frames(p):
    out = []
    cur = None
    with pathlib.Path(p).open(encoding="utf-8") as fh:
        for line in fh:
            if line.startswith("trig: TRIG len="):
                cur = []
                out.append(cur)
            elif line.startswith("trig: TRIG ") and cur is not None:
                cur += [int(x, 16) for x in line[11:].split()]
    return out


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
