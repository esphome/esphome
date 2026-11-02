"""Build the small test images the HTTP server serves."""

from pathlib import Path
import struct
import sys

from PIL import Image

out = Path(sys.argv[1])
out.mkdir(exist_ok=True)
w = h = 16
im = Image.new("RGB", (w, h))
px = im.load()
for y in range(h):
    for x in range(w):
        px[x, y] = (x * 16, y * 16, (x + y) * 8)
im.save(out / "ok.bmp", "BMP")
im.save(out / "ok.png", "PNG")
im.save(out / "ok.jpg", "JPEG", quality=90)

# QOI with only QOI_OP_RGB codes: valid and trivially encoded.
qoi = b"qoif" + struct.pack(">IIBB", w, h, 3, 0)
for y in range(h):
    for x in range(w):
        qoi += bytes([0xFE, *px[x, y]])
qoi += bytes(7) + b"\x01"
(out / "ok.qoi").write_bytes(qoi)

# 8x8 8-bit palette BMP: 1078 byte header, pixels start beyond a 256 byte buffer.
pw = ph = 8
data_offset = 14 + 40 + 256 * 4
file_size = data_offset + pw * ph
hdr = b"BM" + struct.pack("<IHHI", file_size, 0, 0, data_offset)
dib = struct.pack("<IiiHHIIiiII", 40, pw, ph, 1, 8, 0, pw * ph, 0, 0, 256, 0)
(out / "large_header.bmp").write_bytes(hdr + dib + bytes(256 * 4) + bytes(pw * ph))

for p in sorted(out.iterdir()):
    print(p.name, p.stat().st_size)
