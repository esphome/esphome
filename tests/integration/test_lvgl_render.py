"""Integration tests that check what LVGL actually draws, using displays with no screen.

Both tests use one fixture, so LVGL is compiled once for them; they are in one file so CI runs
them on the same runner, where that build is shared. Each draws on its own display, driven by its
own LVGL instance.

Rendered screens are compared against hashes rather than checked in reference images, so the
repository does not have to carry binary files. If a change to the drawing code or to the bundled
LVGL alters the output, a test fails, keeps the image it saw and prints its hash; update the hash
once the new image has been looked at and found to be correct.

The pictures are drawn and encoded entirely by code in this repository, so nothing installed on
the machine running the test takes part in the result.
"""

from __future__ import annotations

import asyncio
import hashlib
from pathlib import Path

import pytest

from .artifact_utils import keep_artifact
from .bmp_utils import Bmp, capture_when_drawn, wait_for_bmp
from .state_utils import require_service
from .types import APIClientConnectedFactory, RunCompiledFunction

# sha256 of the pixel data of a 300x300 screen showing "Hello World!" centred in white on a dark
# blue background, drawn with the built in montserrat_14 font.
HELLO_SIZE = (300, 300)
EXPECTED_HELLO_SHA256 = (
    "a995b002dd1d183c47514da15ab9a60a3e7d788c2e24386a02fddd48655092ed"
)

# Colour pickers drawn with every slider, with a colour wheel, at the size taken from the font,
# and with only some of the sliders, on a 680x480 screen.
PICKER_SIZE = (680, 480)
# Every picker showing 0x3366CC.
EXPECTED_PICKER_INITIAL_SHA256 = (
    "5d93f4bd98fcbe418ef8d34572e3b0912e0421cfd87e61e8cc3c49778a0e0e47"
)
# The first two pickers showing 0xFF8800, and the hue-only one red rather than stuck at white,
# after the update action.
EXPECTED_PICKER_UPDATED_SHA256 = (
    "8300a6d95f38e60ea3d175aa413e71b99de6ac7ad78c9adf5248bfb350cb35d3"
)

# Bundled LVGL version (esphome/components/lvgl/__init__.py, LVGL_VERSION) the hashes above were
# generated against. A version bump can shift anti-aliasing enough to change a hash even though
# nothing is actually wrong -- if a test fails, check that first before regenerating the hash.
EXPECTED_LVGL_VERSION = "9.6.0"

REDRAW_TIMEOUT = 5.0


def _check(
    image: Bmp, capture: Path, size: tuple[int, int], expected: str, name: str
) -> None:
    assert (image.width, image.height, image.bits) == (*size, 24)
    digest = hashlib.sha256(image.pixels).hexdigest()
    if digest == expected:
        return
    # Kept outside the temporary folder so CI can upload it; see artifact_utils.
    kept = keep_artifact(f"lvgl_{name}_actual.bmp", capture.read_bytes())

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
        f"the image that was rendered has been kept at {kept}\n"
        f"on CI it is in the integration-test-artifacts upload for this job"
    )


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_render")
async def test_lvgl_headless_render(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """LVGL draws the expected screen on a 300x300 display with no screen behind it."""
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        service = require_service(services, "take_hello_screenshot")

        async def take(name: str) -> None:
            await client.execute_service(service, {"name": name})

        # The background is not the whole picture: LVGL must have drawn on it. Waiting for that
        # rather than for a fixed time keeps a slow first frame from being reported as a hash
        # mismatch, which would look like a drawing regression.
        image, capture = await capture_when_drawn(take, snapshot_dir, prefix="render")
        _check(image, capture, HELLO_SIZE, EXPECTED_HELLO_SHA256, "headless_render")


@pytest.mark.asyncio
@pytest.mark.shared_yaml("lvgl_render")
async def test_lvgl_color_picker_render(
    yaml_config: str,
    run_compiled: RunCompiledFunction,
    api_client_connected: APIClientConnectedFactory,
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The colour picker draws as expected, before and after its colour is changed.

    The first two pickers are given a new colour by `lvgl.color_picker.update`, and the hue-only
    one is given white, and they are drawn again.
    """
    snapshot_dir = tmp_path / "snapshots"
    monkeypatch.setenv("ESPHOME_SNAPSHOT_DIR", str(snapshot_dir))

    async with run_compiled(yaml_config), api_client_connected() as client:
        _, services = await client.list_entities_services()
        take_screenshot = require_service(services, "take_picker_screenshot")
        set_orange = require_service(services, "set_orange")

        async def take(name: str) -> None:
            await client.execute_service(take_screenshot, {"name": name})

        image, capture = await capture_when_drawn(take, snapshot_dir, prefix="initial")
        _check(
            image,
            capture,
            PICKER_SIZE,
            EXPECTED_PICKER_INITIAL_SHA256,
            "color_picker_initial",
        )

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
        _check(
            updated,
            capture,
            PICKER_SIZE,
            EXPECTED_PICKER_UPDATED_SHA256,
            "color_picker_updated",
        )
