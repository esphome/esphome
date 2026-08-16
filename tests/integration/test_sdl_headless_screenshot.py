"""Integration test for headless SDL rendering and snapshot capture.

How a file is named and written is the same for every display that can take a snapshot and is
covered by test_snapshot_display; what is tested here is that SDL renders and can be read back
with no display server present.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from .bmp_utils import wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 101
HEIGHT = 64


@pytest.mark.asyncio
async def test_sdl_headless_screenshot(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A headless SDL display renders with no display server and can be captured."""
    snapshot_dir = tmp_path / "snapshots"
    # The device reads this when it writes a file; the subprocess inherits our environment, so it
    # must be set before the binary is launched.
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))
    # Make sure the run really is headless even when the test machine has a display.
    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        service = next(s for s in services if s.name == "take_screenshot")

        await client.execute_service(service, {})

        image = await wait_for_bmp(snapshot_dir / "capture.bmp")
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)
        # The test card is drawn in several colours, so a picture of it is not one flat shade.
        # Count whole pixels rather than byte values, which would find more than one of those in
        # even a blank screen.
        colours = {image.pixels[i : i + 3] for i in range(0, len(image.pixels), 3)}
        assert len(colours) > 1
