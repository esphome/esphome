"""Integration test for headless SDL rendering and snapshot capture.

How a file is named and written is the same for every display that can take a snapshot and is
covered by test_snapshot_display; what is tested here is that SDL renders and can be read back
with no display server present.
"""

from __future__ import annotations

from pathlib import Path

import pytest

from .bmp_utils import capture_when_drawn
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

        async def take(name: str) -> None:
            await client.execute_service(service, {"name": name})

        # The test card is drawn in several colours, so once it is on the screen the picture is
        # not one flat shade. Capturing until that is true waits out the first update rather than
        # racing it.
        image, _ = await capture_when_drawn(take, snapshot_dir)
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)
