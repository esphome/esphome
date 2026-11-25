"""ana.py CAPTURE VARIANT PLATFORM: check a recdisp capture against host expectations (esphome#23229, #23226).

Run it in the directory holding the mkimg.py images. VARIANT is base, pr29 (#23229 alone), pr26 (#23226 alone) or
combo (both); PLATFORM e8 is the ESP8266 (per-pixel path), anything else the bulk path; on `host` the RGB565
scene may be one count off per channel. The expect() rules encode what those two PRs changed; adapt them for another
image or display change. Prints ALL_OK=True when every scene matches.
"""

from collections import Counter, defaultdict
import pathlib
import re
import sys

from PIL import Image

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
from lognorm import normalize  # noqa: E402

cap, variant, plat = sys.argv[1:4]
if variant not in ("base", "pr29", "pr26", "combo"):
    sys.exit(f"unknown VARIANT {variant}")
PR29 = variant in ("pr29", "combo")
PR26 = variant in ("pr26", "combo")
E8 = plat == "e8"
HOST = plat == "host"
G = defaultdict(dict)
R = defaultdict(dict)
S = {}
HG = {}
L = {}
T = {}
for line in pathlib.Path(cap).read_text(encoding="latin-1").splitlines():
    if "[rec" not in line:
        continue
    m = re.search(r"\[rec\]: (\w+) (\S+) (.*)$", normalize(line).strip())
    if not m:
        continue
    try:  # a line cut short by a lossy capture is skipped; the completeness checks catch the gap
        k, n, rest = m.groups()
        if k in ("G", "R"):
            y, hx = rest.split(" ", 1)
            px = [
                None
                if hx[i : i + 8] == "--------"
                else tuple(int(hx[i + j : i + j + 2], 16) for j in (0, 2, 4, 6))
                for i in range(0, len(hx) // 8 * 8, 8)
            ]
            (G if k == "G" else R)[n][int(y)] = px
        elif k == "S":
            S[n] = dict(kv.split("=") for kv in rest.split())
        elif k == "Hg":
            HG[n] = [int(v) for v in rest.strip(",").split(",")]
        elif k == "L" and "gray4=" in rest:
            L[n] = rest
        elif k == "T":
            T[n] = dict(kv.split("=") for kv in rest.split())
    except ValueError:
        continue


def bc(f, t, a):
    return (f * (255 - a) + t * a) // 255


def grad(f, t, a):
    return tuple(bc(f[i], t[i], a) for i in range(4))


def luma(c):
    return (77 * c[0] + 150 * c[1] + 29 * c[2] + 128) >> 8


def g4(c):
    return (max(c[3], luma(c)) if PR26 else c[3]) * 16 // 256


ON = (255, 255, 255, 255)
OFF = (0, 0, 0, 0)
KIND = {
    "gray": "g",
    "gray_ck": "gck",
    "gray_a": "ga",
    "agrad": "ga",
    "565": "565",
    "565_ck": "rgbx",
    "565_a": "rgbx",
    "rgb_a": "rgbx",
    "rgb": "rgb",
    "bin": "bin",
    "rgb111_a": "rgbx",
}
# scenes that draw another scene's image with explicit on and off colors: (source scene, on, off)
EXPLICIT = {
    "agrad_wonly": ("agrad", (0, 0, 0, 255), OFF),
    "agrad_rgbw": ("agrad", (200, 30, 90, 170), (10, 250, 60, 5)),
    "gray_a_wonly": ("gray_a", (0, 0, 0, 255), OFF),
    "gray_a_rgbw": ("gray_a", (200, 30, 90, 170), (10, 250, 60, 5)),
}


def expect(kind, P, on=ON, off=OFF):
    r, g, b, w = P
    if kind == "g":
        return (r, g, b, 0) if PR29 else P
    if kind == "gck":
        return None if w == 0 else ((r, g, b, 0) if PR29 else P)
    if kind == "ga":
        g = grad(off, on, w)
        return g if PR29 else g[:3] + (255,)
    if kind in ("565", "rgb"):  # the bulk path always clears white
        return (r, g, b, 0) if PR29 or not E8 else P
    if kind == "rgbx":
        return None if w < 0x80 else ((r, g, b, 0) if PR29 else P)
    if kind == "bin":
        return P
    raise ValueError(kind)


out = []
allok = True
# get_pixel sanity against the PNGs
ring = Image.open("ring_L.png")
agr = Image.open("agrad.png")
ra = Image.open("ring_alpha.png")
try:  # a lost or cut G row fails the check instead of raising
    gp_ok = (
        all(
            G["gray"][y][x][:3] == (ring.getpixel((x, y)),) * 3
            for y in range(ring.height)
            for x in range(ring.width)
        )
        and all(
            G["agrad"][y][x] == (0, 0, 0, agr.getpixel((x, y))[3])
            for y in range(agr.height)
            for x in range(agr.width)
        )
        and all(
            G["gray_a"][y][x] == (0, 0, 0, ra.getpixel((x, y))[3])
            for y in range(ra.height)
            for x in range(ra.width)
        )
    )
except (KeyError, IndexError, TypeError):
    gp_ok = False
out.append(f"get_pixel matches PNG: {gp_ok}")
allok &= gp_ok
# a lost log line must fail the run, not drop a scene or row from the comparison
incomplete = set()
for n in [*KIND, *EXPLICIT]:
    gp = G[EXPLICIT.get(n, (n,))[0]]
    rows = R.get(n, {})
    if (
        not gp
        or not {"drawn", "w", "on"} <= S.get(n, {}).keys()
        or n not in HG
        or rows.keys() != gp.keys()
        or any(len(rows[y]) != len(gp[y]) for y in gp)
    ):
        allok = False
        incomplete.add(n)
        out.append(f"{n:13s} INCOMPLETE capture")
if len(L) != 6:
    allok = False
    out.append(f"fill levels: {len(L)} of 6 logged")
for n, rows in R.items():
    if n in incomplete or (n not in KIND and n not in EXPLICIT):
        continue
    src, on, off = EXPLICIT.get(n, (n, ON, OFF))
    kind = KIND[src]
    near = HOST and kind == "565"
    gp = G[src]
    bad = 0
    wnz = 0
    on_n = 0
    black_on = 0
    drawn = 0
    first = None
    hist = Counter()
    for y, row in rows.items():
        for x, c in enumerate(row):
            e = expect(kind, gp[y][x], on, off)
            if c != e and not (
                near
                and c
                and e
                and all(abs(a - b) <= 1 for a, b in zip(c, e, strict=True))
            ):
                bad += 1
                first = first or (x, y, c, e)
            if c is None:
                continue
            hist[g4(c)] += 1
            drawn += 1
            wnz += c[3] != 0
            on_n += any(c)
            if gp[y][x][:3] == (0, 0, 0) and any(c):
                black_on += 1
    hok = HG[n] == [hist.get(i, 0) for i in range(16)]
    sok = (
        int(S[n]["drawn"]) == drawn
        and int(S[n]["w"]) == wnz
        and int(S[n]["on"]) == on_n
    )
    allok &= bad == 0 and hok and sok
    out.append(
        f"{n:13s} drawn={drawn:5d} w!=0={wnz:5d} on={on_n:5d} black_on={black_on:5d} mismatch={bad} hist_ok={hok} sum_ok={sok} {first or ''}"
    )
# ring gray4 levels: background, ring, center, for the opaque grayscale and 565 scenes
for n in ("gray", "565", "gray_a", "gray_a_wonly"):
    if n in R and n not in incomplete:
        rr = R[n]

        def lv(x, y, rr=rr):
            return g4(rr[y][x]) if rr[y][x] else None

        out.append(
            f"levels {n:13s} bg={lv(0, 0)} ring={lv(23, 4)} center={lv(23, 23)} hist={HG[n]}"
        )
for n, s in L.items():
    c = tuple(int(s.split()[0][i : i + 2], 16) for i in (0, 2, 4, 6))
    dev = int(re.search(r"gray4=(\d+)", s).group(1))
    ok = dev == g4(c)
    allok &= ok
    out.append(f"fill {n:10s} {s}  host={g4(c)} {'ok' if ok else 'MISMATCH'}")
out.extend(
    f"{n} hist={HG[n]} on={S[n].get('on')} w={S[n].get('w')} drawn={S[n].get('drawn')}"
    for n in ("text_wonly", "text_rgb")
    if n in HG and n in S
)
for n, t in T.items():
    out.append(f"time {n:11s} min={t['min']} med={t['med']}")
out.append(f"ALL_OK={allok}")
print("\n".join(out))
