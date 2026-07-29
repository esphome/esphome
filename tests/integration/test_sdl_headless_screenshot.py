"""Integration test for headless SDL rendering and screenshot capture."""

from __future__ import annotations

import asyncio
from pathlib import Path
import struct

import pytest

from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 101
HEIGHT = 64


def _read_bmp_header(path: Path) -> tuple[int, int, int]:
    """Return (width, height, bits per pixel) from a BMP file."""
    data = path.read_bytes()
    assert data[:2] == b"BM", "not a BMP file"
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    # Rows are padded to a multiple of 4 bytes.
    row_size = ((width * bits + 31) // 32) * 4
    assert len(data) >= 54 + row_size * abs(height), "BMP is truncated"
    return width, abs(height), bits


@pytest.mark.asyncio
async def test_sdl_headless_screenshot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A headless SDL display renders with no display server and can be captured."""
    screenshot_dir = tmp_path / "screenshots"
    # The device reads this when it writes a screenshot; the subprocess inherits our environment,
    # so it must be set before the binary is launched.
    monkeypatch.setenv("ESPHOME_SCREENSHOT_DIR", str(screenshot_dir))
    # Make sure the run really is headless even when the test machine has a display.
    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        service = next(s for s in services if s.name == "take_screenshot")

        await client.execute_service(service, {})

        capture = screenshot_dir / "capture.bmp"
        for _ in range(100):
            if capture.is_file():
                break
            await asyncio.sleep(0.05)
        assert capture.is_file(), f"no screenshot appeared in {screenshot_dir}"

        width, height, bits = _read_bmp_header(capture)
        assert (width, height, bits) == (WIDTH, HEIGHT, 24)

        # A second capture to the same name must fail rather than overwrite the first.
        before = capture.read_bytes()
        await client.execute_service(service, {})
        await asyncio.sleep(0.5)
        assert capture.read_bytes() == before
        assert sorted(p.name for p in screenshot_dir.iterdir()) == ["capture.bmp"]
