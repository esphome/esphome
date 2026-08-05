"""Shared utilities for ESPHome integration tests - reading BMP screenshots."""

from __future__ import annotations

import asyncio
from dataclasses import dataclass
from pathlib import Path
import struct

# Size of the smallest BMP header pair (file header plus BITMAPINFOHEADER).
_MIN_HEADER_SIZE = 54


@dataclass(frozen=True)
class Bmp:
    """A decoded BMP image."""

    width: int
    height: int
    bits: int
    #: Pixel data with the per row padding stripped, so it depends only on the image itself.
    pixels: bytes


class NotABmpError(Exception):
    """The data is not a BMP at all, as opposed to a BMP that is still being written."""


def parse_bmp(data: bytes) -> Bmp | None:
    """Decode a BMP, or return None if the data is not a complete image yet.

    Raises:
        NotABmpError: If the data cannot become a valid BMP however much more is appended.
    """
    # Writes go to the file in order, so a short read is always a prefix of what will be there.
    # Anything wrong in a prefix we have already read is wrong for good, and worth saying now
    # rather than reporting as a timeout later.
    if len(data) >= 2 and data[:2] != b"BM":
        raise NotABmpError(f"expected a BMP, got {data[:2]!r}")
    if len(data) < _MIN_HEADER_SIZE:
        return None
    file_size = struct.unpack_from("<I", data, 2)[0]
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    rows = abs(height)
    row_size = ((width * bits + 31) // 32) * 4
    if width <= 0 or rows == 0 or bits == 0 or offset < _MIN_HEADER_SIZE:
        raise NotABmpError(
            f"BMP header makes no sense: {width}x{height}, {bits} bits, "
            f"pixels at offset {offset}"
        )
    if file_size < offset + row_size * rows:
        raise NotABmpError(
            f"BMP header claims {file_size} bytes, too few for {width}x{rows} "
            f"at {bits} bits"
        )
    if len(data) < file_size:
        return None
    used = width * bits // 8
    pixels = b"".join(
        data[offset + row * row_size : offset + row * row_size + used]
        for row in range(rows)
    )
    return Bmp(width=width, height=rows, bits=bits, pixels=pixels)


async def wait_for_bmp(path: Path, timeout: float = 5.0) -> Bmp:
    """Wait for a complete BMP file to appear at ``path`` and return it.

    The file is created before any of its contents are written, so waiting for it to exist is
    not enough - a read that wins the race sees a truncated image. Keep reading until the
    headers say the whole image is there.

    Args:
        path: The file to wait for.
        timeout: Maximum time to wait in seconds.

    Returns:
        The decoded image.

    Raises:
        AssertionError: If no complete image is readable within ``timeout``.
        NotABmpError: If what was written is not a BMP. This is reported as soon as it is
            seen, so a device that writes the wrong thing is named for what it did rather
            than waiting out the timeout.
    """
    loop = asyncio.get_running_loop()
    deadline = loop.time() + timeout
    while True:
        try:
            data = path.read_bytes()
        except FileNotFoundError:
            data = b""
        if (image := parse_bmp(data)) is not None:
            return image
        if loop.time() >= deadline:
            break
        await asyncio.sleep(0.05)
    if not data:
        raise AssertionError(f"no screenshot appeared at {path} within {timeout}s")
    raise AssertionError(
        f"{path} was still incomplete after {timeout}s ({len(data)} bytes)"
    )
