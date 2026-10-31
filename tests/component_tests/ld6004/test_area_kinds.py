"""The LD6004 sizes its dwell areas only when an entity uses them."""

from collections.abc import Callable
from pathlib import Path

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


def test_area_kinds(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    path = tmp_path / "test.yaml"
    path.write_text(BASE)
    generate_main(path)
    defines = {define.name: str(define.value) for define in CORE.defines}
    assert defines["LD600X_AREA_KINDS"] == "2"
    assert defines["LD600X_MAX_TARGETS"] == "3"
