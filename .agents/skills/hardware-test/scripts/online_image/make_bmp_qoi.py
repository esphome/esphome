"""Write big BMP (1, 8, 24 bit) and QOI test images into a www directory."""

from pathlib import Path
import random
import struct
import sys

from PIL import Image

www = Path(sys.argv[1])


def gradient(w, h):
    im = Image.new("RGB", (w, h))
    im.putdata(
        [
            ((x * 255) // w, (y * 255) // h, ((x + y) * 255) // (w + h))
            for y in range(h)
            for x in range(w)
        ]
    )
    return im


def checker(w, h):
    im = Image.new("1", (w, h))
    im.putdata(
        [255 if ((x // 16) + (y // 16)) % 2 else 0 for y in range(h) for x in range(w)]
    )
    return im


checker(640, 480).save(www / "bmp1_640.bmp")

gradient(320, 240).convert("P", palette=Image.ADAPTIVE).save(www / "bmp8_320.bmp")
gradient(320, 240).save(www / "bmp24_320.bmp")


def qoi(im, path):
    im = im.convert("RGBA")
    w, h = im.size
    out = bytearray(b"qoif" + struct.pack(">IIBB", w, h, 4, 0))
    idx = [(0, 0, 0, 0)] * 64
    prev = (0, 0, 0, 255)
    run = 0
    px = list(im.getdata())
    for i, p in enumerate(px):
        if p == prev:
            run += 1
            if run == 62 or i == len(px) - 1:
                out.append(0xC0 | (run - 1))
                run = 0
            continue
        if run:
            out.append(0xC0 | (run - 1))
            run = 0
        h_ = (p[0] * 3 + p[1] * 5 + p[2] * 7 + p[3] * 11) % 64
        if idx[h_] == p:
            out.append(h_)
        else:
            idx[h_] = p
            if p[3] == prev[3]:
                dr, dg, db = (
                    ((p[0] - prev[0] + 128) % 256) - 128,
                    ((p[1] - prev[1] + 128) % 256) - 128,
                    ((p[2] - prev[2] + 128) % 256) - 128,
                )
                drg, dbg = dr - dg, db - dg
                if -2 <= dr <= 1 and -2 <= dg <= 1 and -2 <= db <= 1:
                    out.append(0x40 | (dr + 2) << 4 | (dg + 2) << 2 | (db + 2))
                elif -32 <= dg <= 31 and -8 <= drg <= 7 and -8 <= dbg <= 7:
                    out += bytes([0x80 | (dg + 32), (drg + 8) << 4 | (dbg + 8)])
                else:
                    out += bytes([0xFE, p[0], p[1], p[2]])
            else:
                out += bytes([0xFF, *p])
        prev = p
    out += b"\x00" * 7 + b"\x01"
    path.write_bytes(out)


qoi(
    Image.new("RGB", (640, 480), (0, 0, 0)).copy(), www / "qoi_flat.qoi"
)  # all runs, 62 pixels a byte
qoi(checker(640, 480).convert("RGB"), www / "qoi_check.qoi")
qoi(gradient(320, 240), www / "qoi_grad.qoi")
random.seed(1)
noise = Image.frombytes(
    "RGB", (320, 240), bytes(random.getrandbits(8) for _ in range(320 * 240 * 3))
)
qoi(noise, www / "qoi_noise.qoi")
for f in sorted(www.glob("*_*.*")):
    print(f.name, f.stat().st_size)
