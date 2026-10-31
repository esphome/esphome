"""The LD6004 sizes its dwell areas only when an entity uses them."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

BASE = """
esphome:
  name: test

esp32:
  board: esp32dev
  framework:
    type: esp-idf

uart:
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

ld6004:
  id: radar

binary_sensor:
  - platform: ld6004
    target:
      name: Presence
"""


@pytest.mark.parametrize(
    ("extra", "area_kinds"),
    [
        ("", "2"),
        (
            """
sensor:
  - platform: ld6004
    detection_area_0:
      x_min:
        name: Detection X Min
""",
            "2",
        ),
        (
            """
sensor:
  - platform: ld6004
    dwell_area_1:
      x_min:
        name: Dwell X Min
""",
            "3",
        ),
    ],
)
def test_area_kinds(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    extra: str,
    area_kinds: str,
) -> None:
    path = tmp_path / "test.yaml"
    path.write_text(BASE + extra)
    generate_main(path)
    defines = {define.name: str(define.value) for define in CORE.defines}
    assert defines["LD600X_AREA_KINDS"] == area_kinds
    assert defines["LD600X_MAX_TARGETS"] == "3"
