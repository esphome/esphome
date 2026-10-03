"""Tests for the qspi_dbi init sequence codegen."""

from collections.abc import Callable
from pathlib import Path
import re

_YAML = """
esphome:
  name: qspi-dbi-test
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
psram:
  mode: octal
spi:
  type: quad
  clk_pin: 47
  data_pins: [18, 7, 48, 5]
display:
  - platform: qspi_dbi
    id: builtin
    model: RM690B0
    dimensions:
      width: 450
      height: 600
    cs_pin: 11
  - platform: qspi_dbi
    id: builtin_too
    model: RM690B0
    dimensions:
      width: 450
      height: 600
    cs_pin: 12
  - platform: qspi_dbi
    id: custom
    model: CUSTOM
    dimensions:
      width: 536
      height: 240
    cs_pin: 6
    init_sequence:
      - [0x3A, 0x66]
      - delay 120ms
"""


def test_init_sequence_is_shared_progmem_table(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """Model and custom commands form one PROGMEM table; identical displays share it."""
    yaml_file = tmp_path / "qspi_dbi.yaml"
    yaml_file.write_text(_YAML)

    main_cpp = generate_main(yaml_file)

    calls = {
        m[0]: (m[1], int(m[2]))
        for m in re.findall(r"(\w+)->set_init_sequence\((\w+), (\d+)\);", main_cpp)
    }
    assert calls["builtin"] == calls["builtin_too"]
    assert calls["custom"][0] != calls["builtin"][0]
    custom = re.search(
        rf"static constexpr uint8_t {calls['custom'][0]}\[\] PROGMEM = \{{([^;]*)\}};",
        main_cpp,
    )
    assert custom is not None
    assert custom.group(1).replace(" ", "") == "58,1,102,120,255"
    assert "add_init_sequence" not in main_cpp
