"""Independent models of ESPColorCorrection::color_correct: base (per channel, #18845 lift) and #22868 (pr3).

The gamma table is rebuilt like light/__init__.py does it.
"""

import functools


@functools.cache
def table(g):
    if g > 0:
        return [0] + [
            max(1, min(65535, int(round((i / 255.0) ** g * 65535))))
            for i in range(1, 256)
        ]
    return [int(round(i / 255.0 * 65535)) for i in range(256)]


def s8t(i, a, b):
    return (i * (1 + a) * (1 + b)) >> 16


FULL = (255, 255, 255, 255)


def base(c, lb, t, mb=FULL):
    out = []
    for v, m in zip(c, mb, strict=True):
        lv = t[s8t(v, m, lb)]
        r = (lv + 128) // 257
        out.append(1 if r == 0 and lv != 0 else r)
    return out


def pr3(c, lb, t, has_white=True, mb=FULL):
    c = list(c)
    if not has_white:
        c[3] = 0
    res = [(t[s8t(v, m, lb)] + 128) // 257 for v, m in zip(c, mb, strict=True)]
    if any(res) or not any(c):
        return res
    st = [v * m for v, m in zip(c, mb, strict=True)]
    mx = max(st)
    if not any(s8t(v, m, lb) for v, m in zip(c, mb, strict=True)):
        return [0, 0, 0, 0]
    return [
        1 if x == mx else (1 if x and t[x * 255 // mx] >= 0x8000 else 0) for x in st
    ]
