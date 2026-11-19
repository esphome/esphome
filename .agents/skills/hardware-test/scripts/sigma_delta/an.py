"""an.py CAPTURE: fit the 2 Hz and 50 Hz sine dumps and print the level edges of the underrun, stop and finish dumps.

Reads the "DUMP <label> <i> <t_us>:<density> ..." lines sdprobe.hpp writes (density in 1/10000). Exits 1 when a
dump the gen.py sequence writes is missing, which means the run did not finish.
"""

import math
import pathlib
import re
import sys

DUMPS: dict[str, list[tuple[float, float]]] = {}
for _m in re.finditer(
    r"DUMP (\S+) \d+((?: \d+:\d+)+)",
    pathlib.Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace"),
):
    for _p in _m.group(2).split():
        _t, _d = _p.split(":")
        DUMPS.setdefault(_m.group(1), []).append((int(_t) / 1e6, int(_d) / 100))


def fit(label: str, f0: float) -> bool:
    """Scan +-10% around f0 for the strongest sine; print frequency, amplitude, mean and residual."""
    pts = DUMPS.get(label, [])
    if not pts:
        print(f"{label}: no DUMP lines")
        return False
    n = len(pts)
    mean = sum(d for _, d in pts) / n
    best = None
    for k in range(-200, 201):
        f = f0 * (1 + k * 0.0005)
        s = c = 0.0
        for t, d in pts:
            w = 2 * math.pi * f * t
            s += (d - mean) * math.sin(w)
            c += (d - mean) * math.cos(w)
        a = 2 * math.hypot(s, c) / n
        if best is None or a > best[1]:
            best = (f, a, s, c)
    f, a, s, c = best
    ph = math.atan2(c, s)
    res = math.sqrt(
        sum((d - mean - a * math.sin(2 * math.pi * f * t + ph)) ** 2 for t, d in pts)
        / n
    )
    print(
        f"{label}: n={n} span={pts[-1][0]:.3f}s best f={f:.3f}Hz amplitude={a:.2f}% "
        f"mean={mean:.2f}% rms resid={res:.2f}%"
    )
    return True


def edges(label: str) -> bool:
    """Print each change between high (>70%), mid (45 to 55%) and low (<30%) density."""
    pts = DUMPS.get(label, [])
    print(label, "n", len(pts))
    prev = None
    for t, d in pts:
        lvl = "H" if d > 70 else "M" if 45 < d < 55 else "L" if d < 30 else "?"
        if lvl != prev:
            print(f"  t={t * 1000:8.1f}ms d={d:6.2f}% -> {lvl}")
            prev = lvl
    return bool(pts)


def main() -> int:
    ok = [fit("sine2Hz", 2), fit("sine50Hz", 50)]
    ok += [edges(label) for label in ("underrun", "stop_mid", "finish")]
    if not all(ok):
        print("FAIL: dumps missing")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
