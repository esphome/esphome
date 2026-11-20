"""Tests for the esp32_camera capture_delay option and capture triggers."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

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

START = "  on_capture_start:\n    - logger.log: start\n"
END = "  on_capture_end:\n    - logger.log: end\n"
START_FILTER = "if (!(state))"
END_FILTER = "if (!(!state))"
FILTERS = (START_FILTER, END_FILTER)


@pytest.mark.parametrize(
    ("options", "delay", "filters"),
    [
        ("", None, ()),
        ("  capture_delay: 2s\n", 2000, ()),
        (START, 0, (START_FILTER,)),
        (END, 0, (END_FILTER,)),
        ("  capture_delay: 500ms\n" + START + END, 500, FILTERS),
    ],
)
def test_capture_codegen(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    options: str,
    delay: int | None,
    filters: tuple[str, ...],
) -> None:
    path = tmp_path / "test.yaml"
    path.write_text(BASE + options)
    main_cpp = generate_main(path)

    if delay is None:
        assert "set_capture_delay" not in main_cpp
    else:
        assert f"cam->set_capture_delay({delay});" in main_cpp
    assert main_cpp.count("cam->add_on_capture_callback(") == len(filters)
    for filter_ in FILTERS:
        assert main_cpp.count(filter_) == int(filter_ in filters)
    assert ("USE_ESP32_CAMERA_CAPTURE" in {d.name for d in CORE.defines}) == (
        delay is not None
    )
