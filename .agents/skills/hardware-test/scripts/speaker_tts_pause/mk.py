"""Build one 48 kHz mono FLAC stream from an intro and a continuation, and record where the intro ends.

usage: mk.py INTRO_AUDIO CONT_AUDIO OUT.flac   (any format ffmpeg reads, e.g. from `say -o intro.aiff "..."`)

The intro is padded to a 4096-sample FLAC frame boundary. OUT.flac.cut gets "BYTE_OFFSET INTRO_SAMPLES
TOTAL_SAMPLES": the byte offset of the first frame after the intro, which srv.py uses as the split point.
"""

import pathlib
import subprocess
import sys
import tempfile
import wave

INTRO, CONT, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
BLOCK = 4096


def pcm(src: str, tmp: pathlib.Path) -> bytes:
    dst = tmp / (pathlib.Path(src).stem + ".wav")
    subprocess.run(
        [
            "ffmpeg",
            "-loglevel",
            "error",
            "-y",
            "-i",
            src,
            "-ac",
            "1",
            "-ar",
            "48000",
            "-sample_fmt",
            "s16",
            str(dst),
        ],
        check=True,
    )
    with wave.open(str(dst)) as w:
        return w.readframes(w.getnframes())


def crc8(b: bytes) -> int:
    c = 0
    for x in b:
        c ^= x
        for _ in range(8):
            c = ((c << 1) ^ 0x07) & 0xFF if c & 0x80 else (c << 1) & 0xFF
    return c


with tempfile.TemporaryDirectory() as td:
    tmp = pathlib.Path(td)
    a, b = pcm(INTRO, tmp), pcm(CONT, tmp)
    blocks = -(-len(a) // 2 // BLOCK)
    n = blocks * BLOCK
    a += b"\0" * (n * 2 - len(a))
    with wave.open(str(tmp / "f.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(48000)
        w.writeframes(a + b)
    d = subprocess.run(
        [
            "ffmpeg",
            "-loglevel",
            "error",
            "-i",
            str(tmp / "f.wav"),
            "-c:a",
            "flac",
            "-frame_size",
            str(BLOCK),
        ]
        + ["-f", "flac", "pipe:1"],
        check=True,
        capture_output=True,
    ).stdout
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
