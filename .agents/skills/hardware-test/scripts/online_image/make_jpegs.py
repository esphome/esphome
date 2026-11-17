"""Build the JPEGs for the streaming decode test: noise 320x240 (also ok.jpg) and 640x480, a 640x480 gradient, the
320x240 noise with a 12 KB EXIF segment in front of the frame header, and a progressive one."""

from pathlib import Path
import random
import sys

from PIL import Image

www = Path(sys.argv[1])
random.seed(2)


def noise(w: int, h: int) -> Image.Image:
    im = Image.new("RGB", (w, h))
    px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (
                random.randrange(256),
                random.randrange(256),
                random.randrange(256),
            )
    return im


n320 = noise(320, 240)
n320.save(www / "noise320.jpg", "JPEG", quality=90)
n320.save(www / "ok.jpg", "JPEG", quality=90)
noise(640, 480).save(www / "noise640.jpg", "JPEG", quality=85)
g = Image.new("RGB", (640, 480))
px = g.load()
for y in range(480):
    for x in range(640):
        px[x, y] = (x * 255 // 639, y * 255 // 479, 128)
g.save(www / "gradient640.jpg", "JPEG", quality=85)
n320.save(
    www / "bigexif320.jpg", "JPEG", quality=90, exif=b"Exif\x00\x00" + bytes(12000)
)
n320.save(www / "progressive320.jpg", "JPEG", quality=90, progressive=True)
for p in sorted(www.iterdir()):
    print(p.name, p.stat().st_size)
