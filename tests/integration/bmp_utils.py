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


def parse_bmp(data: bytes) -> Bmp | None:
    """Decode a BMP, or return None if the data is not a complete image yet."""
    if len(data) < _MIN_HEADER_SIZE or data[:2] != b"BM":
        return None
    file_size = struct.unpack_from("<I", data, 2)[0]
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    rows = abs(height)
    row_size = ((width * bits + 31) // 32) * 4
    if len(data) < max(file_size, offset + row_size * rows):
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
