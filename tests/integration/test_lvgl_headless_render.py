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

import pytest

from .artifact_utils import keep_artifact
from .bmp_utils import wait_for_bmp
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 300
HEIGHT = 300

# How long to keep capturing while waiting for LVGL to draw its first frame.
DRAW_TIMEOUT = 15.0

# sha256 of the pixel data of a 300x300 screen showing "Hello World!" centred in white on a dark
# blue background, drawn with the built in montserrat_14 font. To regenerate, run this test and
# take the hash it reports.
EXPECTED_SHA256 = "a995b002dd1d183c47514da15ab9a60a3e7d788c2e24386a02fddd48655092ed"
# Bundled LVGL version (esphome/components/lvgl/__init__.py, LVGL_VERSION) the hash above was
# generated against. A version bump can shift anti-aliasing enough to change the hash even though
# nothing is actually wrong -- if this test fails, check that first before regenerating the hash.
EXPECTED_LVGL_VERSION = "9.5.0"
# The hash covers more than LVGL's drawing: the screen is captured through SDL, which expands the
# RGB565 framebuffer to the 24 bit BMP. SDL comes from the runner's package archive rather than
# from this repository, so an updated image can change the result on its own. Check the SDL
# version too before concluding that a drawing change is at fault.


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

        # Capture until LVGL has drawn something, rather than waiting a fixed time and hoping. A
        # capture taken before the first frame is a plain background, and comparing that against
        # the hash would report a drawing regression when the real problem was timing.
        loop = asyncio.get_running_loop()
        deadline = loop.time() + DRAW_TIMEOUT
        attempt = 0
        while True:
            attempt += 1
            name = f"render-{attempt}.bmp"
            await client.execute_service(service, {"name": name})
            capture = screenshot_dir / name
            image = await wait_for_bmp(capture)
            assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)
            # The background is not the whole picture: something must have been drawn on it.
            # Count whole pixels, not bytes - the background alone is made of the byte values
            # 0x00 and 0x80, so counting bytes would find two of them and pass on a blank screen.
            colours = {image.pixels[i : i + 3] for i in range(0, len(image.pixels), 3)}
            if len(colours) > 1:
                break
            if loop.time() >= deadline:
                pytest.fail(
                    f"the screen was still a single flat colour after {DRAW_TIMEOUT}s "
                    f"and {attempt} captures - LVGL drew nothing"
                )
            await asyncio.sleep(0.5)

        digest = hashlib.sha256(image.pixels).hexdigest()
        if digest != EXPECTED_SHA256:
            # Kept outside the temporary folder so CI can upload it; see artifact_utils.
            kept = keep_artifact(
                "lvgl_headless_render_actual.bmp", capture.read_bytes()
            )

            from esphome.components.lvgl import LVGL_VERSION

            version_hint = ""
            if LVGL_VERSION != EXPECTED_LVGL_VERSION:
                version_hint = (
                    f"the bundled LVGL version changed ({EXPECTED_LVGL_VERSION} -> "
                    f"{LVGL_VERSION}), which is the likely cause\n"
                )
            pytest.fail(
                f"rendered screen does not match the expected hash\n"
                f"{version_hint}"
                f"  expected: {EXPECTED_SHA256}\n"
                f"  actual:   {digest}\n"
                f"the image that was rendered has been kept at {kept}\n"
                f"on CI it is in the integration-test-artifacts upload for this job\n"
                f"if the drawing looks right, check whether the installed SDL version "
                f"changed - the capture goes through it"
            )
