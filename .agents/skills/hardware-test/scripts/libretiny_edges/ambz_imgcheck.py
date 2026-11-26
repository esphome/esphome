"""ambz_imgcheck.py IMAGE [IMAGE...]: is an AmebaZ OTA image's RAM section (load address 0x10005000) complete?

Prints the size the section header declares, where that payload ends, the file length, and how many bytes the file
is short by (positive means the last bytes of `.ram_image2.data` are missing and boot as 0xFF).
"""

import pathlib
import struct
import sys

for name in sys.argv[1:]:
    b = pathlib.Path(name).read_bytes()
    i = b.rfind(struct.pack("<I", 0x10005000) + b"\xff" * 8)
    if i < 4:
        print(pathlib.Path(name).name, "no RAM section header found")
        continue
    size = struct.unpack_from("<I", b, i - 4)[0]
    start = i - 4 + 0x18
    print(
        pathlib.Path(name).name,
        "ram size",
        hex(size),
        "payload end",
        hex(start + size),
        "file len",
        hex(len(b)),
        "short by",
        start + size - len(b),
    )
