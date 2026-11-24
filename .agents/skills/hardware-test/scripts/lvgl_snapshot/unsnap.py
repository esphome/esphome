"""unsnap.py LOG OUTPREFIX: rebuild one PNG per SNAPBEGIN..SNAPEND block of a serial log (2x scale).

Checks the pixel count and the checksum; missing lines are filled with magenta.
"""

import pathlib
import re
import sys

from PIL import Image

log, pre = sys.argv[1], sys.argv[2]
cur = None
for line in (
    pathlib.Path(log).read_text(encoding="utf-8", errors="replace").splitlines()
):
    m = re.search(r"SNAPBEGIN tag=(\d+) w=(\d+) h=(\d+)", line)
    if m:
        cur = {"tag": int(m[1]), "w": int(m[2]), "h": int(m[3]), "lines": {}}
        continue
    m = re.search(r"SNAP (\d+) (\d+) ([0-9a-f]+)", line)
    if m and cur and int(m[1]) == cur["tag"]:
        cur["lines"][int(m[2])] = m[3]
        continue
    m = re.search(r"SNAPEND tag=(\d+) lines=(\d+) runs=(\d+) sum=(\d+)", line)
    if m and cur and int(m[1]) == cur["tag"]:
        n = int(m[2])
        missing = [i for i in range(n) if i not in cur["lines"]]
        px = []
        s = 0
        for i in range(n):
            h = cur["lines"].get(i, "")
            for k in range(0, len(h), 8):
                cnt, col = int(h[k : k + 4], 16), int(h[k + 4 : k + 8], 16)
                s += cnt * 31 + col
                px += [col] * cnt
        w, hh = cur["w"], cur["h"]
        ok = len(px) == w * hh and (s & 0xFFFFFFFF) == int(m[4])
        img = Image.new("RGB", (w, hh))
        data = [
            ((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3) for c in px[: w * hh]
        ]
        data += [(255, 0, 255)] * (w * hh - len(data))
        img.putdata(data)
        img = img.resize((w * 2, hh * 2), Image.NEAREST)
        fn = f"{pre}-snap{cur['tag']}.png"
        img.save(fn)
        print(
            fn,
            "pixels",
            len(px),
            "of",
            w * hh,
            "checksum ok" if ok else "CHECKSUM BAD",
            "missing",
            len(missing),
        )
        cur = None
