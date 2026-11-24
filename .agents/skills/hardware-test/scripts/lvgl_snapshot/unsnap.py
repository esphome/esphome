"""unsnap.py LOG OUTPREFIX: rebuild one PNG per SNAPBEGIN..SNAPEND block of a serial log (2x scale).

Checks the pixel count and the checksum; pixels of missing lines stay magenta in place.
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
    m = re.search(r"SNAP (\d+) (\d+) (\d+) ([0-9a-f]+)", line)
    if m and cur and int(m[1]) == cur["tag"]:
        cur["lines"][int(m[2])] = (int(m[3]), m[4])
        continue
    m = re.search(r"SNAPEND tag=(\d+) lines=(\d+) runs=(\d+) sum=(\d+)", line)
    if m and cur and int(m[1]) == cur["tag"]:
        w, hh = cur["w"], cur["h"]
        missing = [i for i in range(int(m[2])) if i not in cur["lines"]]
        data = [(255, 0, 255)] * (w * hh)
        s = got = 0
        for pos, h in cur["lines"].values():
            for k in range(0, len(h) // 8 * 8, 8):
                cnt, c = int(h[k : k + 4], 16), int(h[k + 4 : k + 8], 16)
                s += cnt * 31 + c
                rgb = ((c >> 11) << 3, ((c >> 5) & 63) << 2, (c & 31) << 3)
                data[pos : pos + cnt] = [rgb] * cnt
                pos += cnt
                got += cnt
        del data[w * hh :]
        ok = got == w * hh and (s & 0xFFFFFFFF) == int(m[4])
        img = Image.new("RGB", (w, hh))
        img.putdata(data)
        img = img.resize((w * 2, hh * 2), Image.NEAREST)
        fn = f"{pre}-snap{cur['tag']}.png"
        img.save(fn)
        print(
            fn,
            "pixels",
            got,
            "of",
            w * hh,
            "checksum ok" if ok else "CHECKSUM BAD",
            "missing lines",
            missing[:10],
        )
        cur = None
