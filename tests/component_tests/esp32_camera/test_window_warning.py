"""Tests for the esp32_camera window bounds warning."""

from collections.abc import Callable
from pathlib import Path

import pytest

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
"""


@pytest.mark.parametrize(
    ("resolution", "window", "expected"),
    [
        # A window that exactly fills the frame stays quiet.
        ("QVGA", (0, 0, 320, 240), ""),
        # Filling a larger frame from an offset fits.
        ("VGA", (100, 0, 320, 240), ""),
        # A window inside the frame stays quiet.
        ("QVGA", (100, 0, 200, 200), ""),
        # One pixel of overflow on the height axis, full-resolution window.
        ("QVGA", (0, 1, 320, 240), "so the window is fine there"),
        # A full-resolution window with an offset is valid on the SC sensors.
        ("320X240", (100, 0, 320, 240), "so the window is fine there"),
        # The same frame size spelled as an alias must be checked as well.
        ("QVGA", (100, 0, 320, 240), "so the window is fine there"),
        # Larger than the frame itself: no sensor can use it.
        ("QVGA", (0, 0, 400, 100), "no sensor can read it out"),
        # Not the resolution and reaching past it: the SC sensors reject it.
        ("QVGA", (100, 0, 300, 240), "cannot be used there"),
    ],
)
def test_window_bounds_warning(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    caplog: pytest.LogCaptureFixture,
    resolution: str,
    window: tuple[int, int, int, int],
    expected: str,
) -> None:
    offset_x, offset_y, width, height = window
    path = tmp_path / "test.yaml"
    path.write_text(
        BASE
        + f"  resolution: {resolution}\n"
        + "  window:\n"
        + f"    offset_x: {offset_x}\n"
        + f"    offset_y: {offset_y}\n"
        + f"    width: {width}\n"
        + f"    height: {height}\n"
    )

    generate_main(path)

    if not expected:
        assert "camera window" not in caplog.text
        return
    assert "camera window" in caplog.text
    assert expected in caplog.text
