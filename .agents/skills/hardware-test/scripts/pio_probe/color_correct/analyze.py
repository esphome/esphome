"""analyze.py BASE_CAPTURE PR_CAPTURE: compare every full probe capture with the base and PR models.

Covers the main strips and the partition segments; prints mismatches, a summary per build, one row per case of
round 1 with both expected frames, and any pixel that should be lit but decodes dark.
"""

import collections
import pathlib
import re
import sys

from model import base, pr3, table

GAMMA_1 = {"rgb10"}  # every other strip and partition uses gamma 2.8
RGBW = {"rgbw28", "rgbwdef", "rgbwmb", "seg_rgbw"}
SEGS = {"seg_rgb": False, "seg_rgbw": True}


def h2c(h):
    return [int(h[i : i + 2], 16) for i in range(0, 8, 2)]


def order(c, rgbw):
    return [c[1], c[0], c[2]] + ([c[3]] if rgbw else [])


def corr(rule, c, lb, t, has_white, mb):
    return base(c, lb, t, mb) if rule == "base" else pr3(c, lb, t, has_white, mb)


def frame(vals, rgbw):
    return "".join(f"{v:02x}" for p in vals for v in order(p, rgbw))


def parse(path, rule):
    exp, res, info = {}, collections.defaultdict(list), {}
    for line in (
        pathlib.Path(path).read_text(encoding="utf-8", errors="replace").splitlines()
    ):
        if m := re.search(
            r"\bIN r(\d) (\S+) (\S+) lb=(\d+) px0=(\w{8}) px1=(\w{8}) mb=(\w{8})", line
        ):
            k = (m[1], m[2], m[3])
            lb = int(m[4])
            s = m[3]
            t = table(1.0 if s in GAMMA_1 else 2.8)
            mb = h2c(m[7])
            px = [h2c(m[5]), h2c(m[6])]
            exp[k] = frame([corr(rule, p, lb, t, s in RGBW, mb) for p in px], s in RGBW)
            info[k] = (lb, px, mb)
        elif m := re.search(r"PIN r(\d) (\S+) (\S+) lb=(\d+) px=(\w{8})", line):
            lb = int(m[4])
            px = h2c(m[5])
            t = table(1.0 if m[3] in GAMMA_1 else 2.8)
            for seg, w in SEGS.items():
                k = (m[1], m[2] + "@" + m[3], seg)
                exp[k] = frame([corr(rule, px, lb, t, w, (255,) * 4)] * 2, w)
                info[k] = (lb, [px, px], None)
        elif m := re.search(
            r"RESULT r(\d) (\S+) (\S+) \d: (\d+) pulses \(expect (\d+)\).*bytes (\w+)",
            line,
        ):
            res[(m[1], m[2], m[3])].append((m[4] == m[5], m[6].lower()))
    return exp, res, info


out = {}
for rule, path in (("base", sys.argv[1]), ("pr", sys.argv[2])):
    exp, res, info = parse(path, rule)
    ok = bad = full = 0
    nomatch = []
    for k, e in exp.items():
        caps = [g for c, g in res[k] if c]
        full += len(caps)
        ok += sum(g == e for g in caps)
        bad += sum(g != e for g in caps)
        for g in caps:
            if g != e:
                print(rule, "MISMATCH", k, "got", g, "exp", e)
        if not any(g == e for g in caps):
            nomatch.append(k)
        out[(rule,) + k] = (info[k], e)
    print(
        f"SUMMARY {rule} keys {len(exp)} full captures {full} match {ok} mismatch {bad} keys without a match {nomatch}"
    )
dark = []
for (rule, rnd, name, strip), ((lb, px, mb), e) in out.items():
    n = 8 if strip in RGBW else 6
    for i, pp in enumerate(px):
        vis = pp if strip in RGBW else pp[:3]
        if mb is not None:
            vis = [v if m else 0 for v, m in zip(vis, mb, strict=False)]
        if any(vis) and int(e[i * n : (i + 1) * n], 16) == 0:
            dark.append((rule, name, strip, i))
    if rule == "base" and rnd == "1":
        p = out.get(("pr", rnd, name, strip), (None, "MISSING"))
        print(
            f"ROW {name:22s} {strip:8s} lb={lb:3d} px1={''.join(f'{x:02x}' for x in px[1])} base {e} pr {p[1]}"
            + ("  DIFF" if p[1] != e else "")
        )
print("dark non-black pixels:", dark)
