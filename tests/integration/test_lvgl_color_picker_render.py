"""Golden-image test for the LVGL colour picker, drawn on a display with no screen.

Two pickers are drawn side by side, one built from sliders and one with a colour wheel, then
both are given a new colour by `lvgl.color_picker.update` and drawn again. Each picture is
compared against a hash, as in test_lvgl_headless_render.py. If a change alters the output,
the test fails, keeps the image it saw and prints its hash; update the hash once the image has
been looked at and found to be correct.
"""

from __future__ import annotations

import asyncio
import hashlib
from pathlib import Path

import pytest

from .artifact_utils import keep_artifact
from .bmp_utils import Bmp, capture_when_drawn, wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 400
HEIGHT = 200

# Both pickers showing 0x3366CC.
EXPECTED_INITIAL_SHA256 = (
    "02e1c9ba55f7dabc8f89337c235f614131d3789a4aa6ff2b0114c40b70014195"
)
# Both pickers showing 0xFF8800, after the update action.
EXPECTED_UPDATED_SHA256 = (
    "794480638172b66f49dca24c04723ac5d7400fab79fa97f5ad75fc254b2d9ab8"
)
# The LVGL version the hashes were generated against; a version change can move them.
EXPECTED_LVGL_VERSION = "9.5.0"

REDRAW_TIMEOUT = 5.0


def _check(image: Bmp, capture: Path, expected: str, name: str) -> None:
    assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)
    digest = hashlib.sha256(image.pixels).hexdigest()
    if digest == expected:
        return
    kept = keep_artifact(f"lvgl_color_picker_{name}_actual.bmp", capture.read_bytes())

    from esphome.components.lvgl import LVGL_VERSION

    version_hint = ""
    if LVGL_VERSION != EXPECTED_LVGL_VERSION:
        version_hint = (
            f"the bundled LVGL version changed ({EXPECTED_LVGL_VERSION} -> "
            f"{LVGL_VERSION}), which is the likely cause\n"
        )
    pytest.fail(
        f"{name} screen does not match the expected hash\n"
        f"{version_hint}"
        f"  expected: {expected}\n"
        f"  actual:   {digest}\n"
        f"the image that was rendered has been kept at {kept}"
    )


@pytest.mark.asyncio
async def test_lvgl_color_picker_render(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The colour picker draws as expected, before and after its colour is changed."""
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        take_screenshot = next(s for s in services if s.name == "take_screenshot")
        set_orange = next(s for s in services if s.name == "set_orange")

        async def take(name: str) -> None:
            await client.execute_service(take_screenshot, {"name": name})

        image, capture = await capture_when_drawn(take, snapshot_dir, prefix="initial")
        _check(image, capture, EXPECTED_INITIAL_SHA256, "initial")

        await client.execute_service(set_orange, {})
        # The redraw follows the update on a later LVGL tick, so capture until the picture changes.
        loop = asyncio.get_running_loop()
        deadline = loop.time() + REDRAW_TIMEOUT
        attempt = 0
        while True:
            attempt += 1
            capture = snapshot_dir / f"updated-{attempt}.bmp"
            await take(capture.name)
            updated = await wait_for_bmp(capture)
            if updated.pixels != image.pixels:
                break
            if loop.time() >= deadline:
                pytest.fail(
                    f"the screen did not change within {REDRAW_TIMEOUT}s of the update"
                )
            await asyncio.sleep(0.2)
        _check(updated, capture, EXPECTED_UPDATED_SHA256, "updated")
