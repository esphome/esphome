"""lvgl.canvas.draw_text with a text computed from an action variable.

The canvas layer draws the label when it is finished, after the statement that set the text.
A text held only in a temporary std::string would be read after it was freed, which
AddressSanitizer reports; the flags are added on Linux, where CI runs it.
"""

from __future__ import annotations

from pathlib import Path
import sys

import pytest

from .bmp_utils import capture_when_drawn
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

# Longer than the small string buffer, so the text is on the heap
TEXT = "A canvas text longer than sixteen characters"


@pytest.mark.asyncio
async def test_lvgl_canvas_text(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Drawing a computed text on a canvas reads it while it is still alive."""
    if sys.platform == "linux":
        yaml_config = yaml_config.replace(
            "build_flags: []",
            'build_flags: ["-fsanitize=address", "-fno-omit-frame-pointer"]',
        )
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        draw_text = require_service(services, "draw_text")
        take_screenshot = require_service(services, "take_screenshot")

        await client.execute_service(draw_text, {"message": TEXT})

        async def take(name: str) -> None:
            await client.execute_service(take_screenshot, {"name": name})

        # Something other than the black background was drawn, and the device is still running
        image, _ = await capture_when_drawn(take, snapshot_dir)
        assert (image.width, image.height) == (120, 40)
