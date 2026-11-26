"""mkimg.py: write the test images scene-common.yaml uses into the current directory.

ring.png is saved as RGB (see display.md); ring_L.png keeps the same pixels in `L` mode for ana.py.
"""

import math

from PIL import Image

N = 48


def ring(mode, bg, outer, center, band):
    """Ring at radius 16 to 22, center disc below 10, band(x, y) colors the 10 to 16 band."""
    im = Image.new(mode, (N, N), bg)
    for y in range(N):
        for x in range(N):
            r = math.hypot(x - 23.5, y - 23.5)
            if 16 <= r < 22:
                im.putpixel((x, y), outer)
            elif r < 10:
                im.putpixel((x, y), center)
            elif r < 16:
                im.putpixel((x, y), band(x, y))
    return im


# white ring, mid-gray center, extra gray levels in the band, black background, opaque
g = ring("L", 0, 255, 128, lambda x, y: (x * 5) % 256)
g.save("ring_L.png")
g.convert("RGB").save("ring.png")
# RGBA: same colors, background transparent, varying alpha in the band
ring(
    "RGBA",
    (0, 0, 0, 0),
    (255, 255, 255, 255),
    (128, 128, 128, 255),
    lambda x, y: (200, (x * 5) % 256, 40, (y * 11) % 256),
).save("ring_rgba.png")
# alpha only ring: black RGB, alpha = ring shape with gradient band
ring(
    "RGBA",
    (0, 0, 0, 0),
    (0, 0, 0, 255),
    (0, 0, 0, 128),
    lambda x, y: (0, 0, 0, (x * 7 + y * 3) % 256),
).save("ring_alpha.png")
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
