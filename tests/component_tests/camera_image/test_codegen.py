"""Tests for the code camera_image generates from the decoded frame size."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.image import get_image_metadata

BASE = """
esphome:
  name: test
esp32:
  board: esp32cam
logger:
psram:
i2c:
  id: camera_bus
  sda: 26
  scl: 27
esp32_camera:
  id: cam
  name: Camera
  data_pins: [5, 18, 19, 21, 36, 39, 34, 35]
  vsync_pin: 25
  href_pin: 23
  pixel_clock_pin: 22
  external_clock:
    pin: 0
    frequency: 20MHz
  i2c_id: camera_bus
  power_down_pin: 32
  resolution: 640X480
image:
  - platform: camera_image
    id: cam_image
    camera_id: cam
    format: JPEG
    type: RGB565
{extra}
display:
  - platform: ssd1306_i2c
    model: "SSD1306 128x64"
    address: 0x3C
    i2c_id: camera_bus
"""


def _options(*lines: str) -> str:
    """Join camera_image option lines for insertion into BASE."""
    return "".join(f"{line}\n" for line in lines)


@pytest.mark.parametrize(
    ("extra", "expected_size", "transformed"),
    [
        # resize only bounds the frame: 640x480 fitted into 320x320 is 320x240,
        # so that is both the size to report and the size the buffer holds.
        (_options("    resize: 320x320"), (320, 240), False),
        # A rotation publishes the decoded size swapped, and needs scratch memory
        # for exactly that, not for the larger resize box.
        (_options("    resize: 320x320", "    rotation: 90"), (240, 320), True),
        (_options("    resize: 320x320", "    rotation: 180"), (320, 240), True),
        # A crop publishes the crop size, swapped by a 90 or 270 degree rotation.
        (
            _options(
                "    resize: 320x320",
                "    crop:",
                "      x: 10",
                "      y: 10",
                "      width: 100",
                "      height: 80",
            ),
            (100, 80),
            True,
        ),
        (
            _options(
                "    resize: 320x320",
                "    crop:",
                "      x: 10",
                "      y: 10",
                "      width: 100",
                "      height: 80",
                "    rotation: 270",
            ),
            (80, 100),
            True,
        ),
        # Without a resize the frame decodes at the camera resolution.
        ("", (640, 480), False),
        # A rotation needs neither: the camera resolution sizes the output and
        # the scratch buffer, here swapped to the 480x640 the rotation publishes.
        (_options("    rotation: 90"), (480, 640), True),
    ],
)
def test_metadata_and_transform_size_follow_the_decoded_frame(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    extra: str,
    expected_size: tuple[int, int],
    transformed: bool,
) -> None:
    path = tmp_path / "test.yaml"
    path.write_text(BASE.format(extra=extra))

    main_cpp = generate_main(path)

    # The metadata is what LVGL and other consumers size the image by.
    metadata = get_image_metadata("cam_image")
    assert metadata is not None
    assert (metadata.width, metadata.height) == expected_size

    transform = f"set_transform_size({expected_size[0]}, {expected_size[1]})"
    if transformed:
        assert transform in main_cpp
    else:
        # Nothing crops or rotates, so there is no output size to set.
        assert "set_transform_size(" not in main_cpp
