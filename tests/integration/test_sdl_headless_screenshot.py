"""Integration test for headless SDL rendering and screenshot capture."""

from __future__ import annotations

import asyncio
from pathlib import Path

from aioesphomeapi import LogLevel
import pytest

from .bmp_utils import wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 101
HEIGHT = 64

# Part of the message write_bmp_() logs when it will not write over a file that is already there.
REFUSAL_MESSAGE = b"not overwriting"


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
        image = await wait_for_bmp(capture)
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)

        # An extension is only added when there is not one already, whatever its case.
        upper = next(s for s in services if s.name == "take_screenshot_upper")
        await client.execute_service(upper, {})
        await wait_for_bmp(screenshot_dir / "UPPER.BMP")

        # A second capture to the same name must fail rather than overwrite the first. Wait for
        # the device to report the refusal: on its own, an unchanged file cannot tell a refusal
        # apart from a request the device has not got to yet, so a regression that overwrote the
        # file could still pass on a busy machine.
        refused = asyncio.Event()

        def on_log(msg) -> None:
            if REFUSAL_MESSAGE in msg.message:
                refused.set()

        client.subscribe_logs(on_log, log_level=LogLevel.LOG_LEVEL_DEBUG)

        before = capture.read_bytes()
        await client.execute_service(service, {})
        await asyncio.wait_for(refused.wait(), timeout=10.0)
        assert capture.read_bytes() == before
        assert sorted(p.name for p in screenshot_dir.iterdir()) == [
            "UPPER.BMP",
            "capture.bmp",
        ]
