"""Tests for the st7701s init sequence codegen."""

from collections.abc import Callable
from pathlib import Path
import re

_YAML = """
esphome:
  name: st7701s-test
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
psram:
  mode: octal
spi:
  clk_pin: 10
  mosi_pin: 11
display:
{displays}
"""

_DISPLAY = """
  - platform: st7701s
    id: {display_id}
    dimensions:
      width: 480
      height: 480
    cs_pin: {cs_pin}
    de_pin: 18
    hsync_pin: 16
    vsync_pin: 17
    pclk_pin: 21
    init_sequence:
      - 1
      - [0x23, 0xA, 0xB]
      - delay 20ms
    data_pins: [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 13, 14, 15, 38, 39]
"""


def test_init_sequence_is_progmem_table(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """The init sequence is a PROGMEM table instead of a vector copied at boot."""
    yaml_file = tmp_path / "st7701s.yaml"
    yaml_file.write_text(
        _YAML.format(displays=_DISPLAY.format(display_id="panel", cs_pin=44))
    )

    main_cpp = generate_main(yaml_file)

    call = re.search(r"panel->set_init_sequence\((\w+), (\d+)\);", main_cpp)
    assert call is not None
    table = re.search(
        rf"static constexpr uint8_t {call.group(1)}\[\] PROGMEM = \{{([^;]*)\}};",
        main_cpp,
    )
    assert table is not None
    assert len(table.group(1).split(",")) == int(call.group(2))
