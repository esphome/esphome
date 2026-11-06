"""Build one 48 kHz mono FLAC stream from an intro and a continuation, and record where the intro ends.

usage: mk.py INTRO_AUDIO CONT_AUDIO OUT.flac   (any format ffmpeg reads, e.g. from `say -o intro.aiff "..."`)

The intro is padded to a 4096-sample FLAC frame boundary. OUT.flac.cut gets "BYTE_OFFSET INTRO_SAMPLES
TOTAL_SAMPLES": the byte offset of the first frame after the intro, which srv.py uses as the split point.
"""

import pathlib
import subprocess
import sys

INTRO, CONT, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
BLOCK = 4096
RAW = ["-f", "s16le", "-ac", "1", "-ar", "48000"]


def ffmpeg(args: list[str], data: bytes | None = None) -> bytes:
    return subprocess.run(
        ["ffmpeg", "-loglevel", "error", *args],
        input=data,
        check=True,
        capture_output=True,
    ).stdout


def pcm(src: str) -> bytes:
    return ffmpeg(["-i", src, *RAW, "pipe:1"])


def crc8(b: bytes) -> int:
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


a, b = pcm(INTRO), pcm(CONT)
blocks = -(-len(a) // 2 // BLOCK)
n = blocks * BLOCK
a += b"\0" * (n * 2 - len(a))
d = ffmpeg(
    [
        *RAW,
        "-i",
        "pipe:0",
        "-c:a",
        "flac",
        "-frame_size",
        str(BLOCK),
        "-f",
        "flac",
        "pipe:1",
    ],
    a + b,
)
# Find the frame header whose frame number equals the intro's block count (fixed blocksize, 1 byte UTF-8 number)
first = d.find(b"\xff\xf8")
h = d[first : first + 4]
i, cut = first, None
while (i := d.find(b"\xff\xf8", i)) >= 0:
    if (
        d[i + 2 : i + 4] == h[2:4]
        and d[i + 4] == blocks
        and crc8(d[i : i + 5]) == d[i + 5]
    ):
        cut = i
        break
    i += 1
if cut is None:
    sys.exit("intro frame boundary not found (intro longer than 127 frames?)")
pathlib.Path(OUT).write_bytes(d)
pathlib.Path(OUT + ".cut").write_text(
    f"{cut} {n} {(len(a) + len(b)) // 2}", encoding="utf-8"
)
print(
    f"{OUT}: {len(d)} B, intro {cut - first} B of frames, {n} intro samples, {(len(a) + len(b)) // 2} total"
)
