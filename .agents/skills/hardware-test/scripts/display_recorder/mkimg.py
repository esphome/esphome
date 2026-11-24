"""mkimg.py: write the test images scene-common.yaml uses into the current directory.

An `L` mode PNG with `type: GRAYSCALE` is read as alpha only, so ring.png is saved as RGB; ring_L.png keeps the
same pixels in `L` mode for ana.py.
"""

import math

from PIL import Image

N = 48


def ring_gray():  # white ring, mid-gray center, black background, opaque
    im = Image.new("L", (N, N), 0)
    for y in range(N):
        for x in range(N):
            r = math.hypot(x - 23.5, y - 23.5)
            if 16 <= r < 22:
                im.putpixel((x, y), 255)
            elif r < 10:
                im.putpixel((x, y), 128)
            elif 10 <= r < 16:
                im.putpixel((x, y), (x * 5) % 256)  # extra gray levels
    return im


g = ring_gray()
g.save("ring_L.png")
g.convert("RGB").save("ring.png")
# RGBA: same colors, background transparent, ring tinted, a varying-alpha band
im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
for y in range(N):
    for x in range(N):
        r = math.hypot(x - 23.5, y - 23.5)
        if 16 <= r < 22:
            im.putpixel((x, y), (255, 255, 255, 255))
        elif r < 10:
            im.putpixel((x, y), (128, 128, 128, 255))
        elif 10 <= r < 16:
            im.putpixel((x, y), (200, (x * 5) % 256, 40, (y * 11) % 256))
im.save("ring_rgba.png")
# alpha only ring: black RGB, alpha = ring shape with gradient band
a = Image.new("RGBA", (N, N), (0, 0, 0, 0))
for y in range(N):
    for x in range(N):
        r = math.hypot(x - 23.5, y - 23.5)
        if 16 <= r < 22:
            a.putpixel((x, y), (0, 0, 0, 255))
        elif r < 10:
            a.putpixel((x, y), (0, 0, 0, 128))
        elif 10 <= r < 16:
            a.putpixel((x, y), (0, 0, 0, (x * 7 + y * 3) % 256))
a.save("ring_alpha.png")
# 16x16 alpha ramp 0..255
b = Image.new("RGBA", (16, 16))
for i in range(256):
    b.putpixel((i % 16, i // 16), (0, 0, 0, i))
b.save("agrad.png")
# RGB111 test: primary colors ring
c = Image.new("RGBA", (N, N), (0, 0, 0, 255))
cols = [
    (255, 0, 0),
    (0, 255, 0),
    (0, 0, 255),
    (255, 255, 0),
    (0, 255, 255),
    (255, 0, 255),
    (255, 255, 255),
    (0, 0, 0),
]
for y in range(N):
    for x in range(N):
        c.putpixel((x, y), cols[(x // 6 + y // 6) % 8] + (255 if (x + y) % 5 else 0,))
c.save("rgb111.png")
