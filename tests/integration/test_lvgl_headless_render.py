"""Integration test that checks what LVGL actually draws, using a headless SDL display.

The rendered screen is compared against a hash rather than a checked in reference image, so the
repository does not have to carry a binary file. If a change to the drawing code or to the bundled
LVGL alters the output, this test fails and prints the hash it saw; update EXPECTED_SHA256 once the
new image has been looked at and found to be correct.
"""

from __future__ import annotations

import asyncio
import hashlib
from pathlib import Path
import struct

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 300
HEIGHT = 300

# sha256 of the pixel data of a 300x300 screen showing "Hello World!" centred in white on a dark
# blue background, drawn with the built in montserrat_14 font. To regenerate, run this test and
# take the hash it reports.
EXPECTED_SHA256 = "a995b002dd1d183c47514da15ab9a60a3e7d788c2e24386a02fddd48655092ed"


def _bmp_pixels(path: Path) -> tuple[int, int, int, bytes]:
    """Return (width, height, bits per pixel, pixel data) from a BMP file.

    The per row padding is stripped, so the returned bytes depend only on the image itself.
    """
    data = path.read_bytes()
    assert data[:2] == b"BM", "not a BMP file"
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    row_size = ((width * bits + 31) // 32) * 4
    used = width * bits // 8
    rows = [
        data[offset + row * row_size : offset + row * row_size + used]
        for row in range(abs(height))
    ]
    assert len(rows[-1]) == used, "BMP is truncated"
    return width, abs(height), bits, b"".join(rows)


@pytest.mark.asyncio
async def test_lvgl_headless_render(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """LVGL draws the expected screen on a headless 300x300 display."""
    screenshot_dir = tmp_path / "screenshots"
    monkeypatch.setenv("ESPHOME_SCREENSHOT_DIR", str(screenshot_dir))
    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        service = next(s for s in services if s.name == "take_screenshot")

        # Give LVGL time to draw the first frame before capturing it.
        await asyncio.sleep(1.0)
        await client.execute_service(service, {})

        capture = screenshot_dir / "hello_world.bmp"
        for _ in range(100):
            if capture.is_file():
                break
            await asyncio.sleep(0.05)
        assert capture.is_file(), f"no screenshot appeared in {screenshot_dir}"

        width, height, bits, pixels = _bmp_pixels(capture)
        assert (width, height, bits) == (WIDTH, HEIGHT, 24)

        # The background is not the whole picture: something must have been drawn on it.
        assert len(set(pixels)) > 1, "the screen is a single flat colour"

        digest = hashlib.sha256(pixels).hexdigest()
        if digest != EXPECTED_SHA256:
            kept = Path.cwd() / "lvgl_headless_render_actual.bmp"
            kept.write_bytes(capture.read_bytes())
            pytest.fail(
                f"rendered screen does not match the expected hash\n"
                f"  expected: {EXPECTED_SHA256}\n"
                f"  actual:   {digest}\n"
                f"the image that was rendered has been kept at {kept}"
            )
