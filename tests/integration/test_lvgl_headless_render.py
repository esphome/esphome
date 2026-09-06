"""Integration test that checks what LVGL actually draws, using a display with no screen.

The rendered screen is compared against a hash rather than a checked in reference image, so the
repository does not have to carry a binary file. If a change to the drawing code or to the bundled
LVGL alters the output, this test fails and prints the hash it saw; update EXPECTED_SHA256 once the
new image has been looked at and found to be correct.

The picture is drawn and encoded entirely by code in this repository, so nothing installed on the
machine running the test takes part in the result.
"""

from __future__ import annotations

import hashlib
from pathlib import Path

import pytest

from .artifact_utils import keep_artifact
from .bmp_utils import capture_when_drawn
from .types import APIClientConnectedFactory, RunCompiledFunction

WIDTH = 300
HEIGHT = 300

# sha256 of the pixel data of a 300x300 screen showing "Hello World!" centred in white on a dark
# blue background, drawn with the built in montserrat_14 font. To regenerate, run this test and
# take the hash it reports.
EXPECTED_SHA256 = "a995b002dd1d183c47514da15ab9a60a3e7d788c2e24386a02fddd48655092ed"
# Bundled LVGL version (esphome/components/lvgl/__init__.py, LVGL_VERSION) the hash above was
# generated against. A version bump can shift anti-aliasing enough to change the hash even though
# nothing is actually wrong -- if this test fails, check that first before regenerating the hash.
EXPECTED_LVGL_VERSION = "9.5.0"


@pytest.mark.asyncio
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
        service = next(s for s in services if s.name == "take_screenshot")

        async def take(name: str) -> None:
            await client.execute_service(service, {"name": name})

        # The background is not the whole picture: LVGL must have drawn on it. Waiting for that
        # rather than for a fixed time keeps a slow first frame from being reported as a hash
        # mismatch, which would look like a drawing regression.
        image, capture = await capture_when_drawn(take, snapshot_dir, prefix="render")
        assert (image.width, image.height, image.bits) == (WIDTH, HEIGHT, 24)

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
                f"on CI it is in the integration-test-artifacts upload for this job"
            )
