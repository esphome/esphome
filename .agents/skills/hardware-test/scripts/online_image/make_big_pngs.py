"""Build the large PNGs for the decode time test: a compressible gradient (also saved as ok.png so every server case
carries a multi-row PNG), a flat 640x480 image whose few KB hold 307200 pixels, incompressible noise, and a JPEG."""

from pathlib import Path
import random
import sys

from PIL import Image

www = Path(sys.argv[1])
im = Image.new("RGB", (320, 240))
px = im.load()
for y in range(240):
    for x in range(320):
        px[x, y] = (x * 255 // 319, y * 255 // 239, 128)
im.save(www / "ok.png", "PNG", optimize=True)
im.save(www / "gradient.png", "PNG", optimize=True)
im.save(www / "gradient.jpg", "JPEG", quality=85)
Image.new("RGB", (640, 480), (40, 90, 200)).save(www / "flat.png", "PNG", optimize=True)
random.seed(1)
noise = Image.new("RGB", (320, 240))
npx = noise.load()
for y in range(240):
    for x in range(320):
        npx[x, y] = (
            random.randrange(256),
            random.randrange(256),
            random.randrange(256),
        )
noise.save(www / "noise.png", "PNG")
for p in sorted(www.iterdir()):
    print(p.name, p.stat().st_size)
