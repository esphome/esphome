"""End-to-end tests for the mipi_rgb SPI reset sequence.

These exercise the actual codegen path (mipi_rgb/display.py's
``model.get_sequence(config, add_reset=True)`` call) rather than calling
DriverChip.get_sequence directly, so a regression that drops add_reset or
reintroduces a hardcoded SWRESET into a model's initsequence would be caught
here.
"""

from collections.abc import Callable
from pathlib import Path
import re

# A model with no reset_pin default: SWRESET ({1, 0}) is prepended ahead of the
# inherited ST7701S reset_delay ({50, 255}).
_NO_RESET_PIN_YAML = """
esphome:
  name: mipi-rgb-reset-test
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
psram:
  mode: octal
spi:
  id: spi_bus
  clk_pin: 10
  mosi_pin: 11
display:
  - platform: mipi_rgb
    id: no_reset_display
    spi_id: spi_bus
    model: MAKERFABS-4
"""

# A model with a reset_pin default: no SWRESET, just the settling delay.
_RESET_PIN_YAML = """
esphome:
  name: mipi-rgb-reset-test
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
psram:
  mode: octal
spi:
  id: spi_bus
  clk_pin: 6
  mosi_pin: 7
display:
  - platform: mipi_rgb
    id: has_reset_display
    spi_id: spi_bus
    model: WAVESHARE-3.16-320X820
"""


def _init_sequence(main_cpp: str, display_id: str) -> str:
    """Return the shared PROGMEM init sequence table passed to a display, as rendered."""
    match = re.search(rf"{display_id}->set_init_sequence\((\w+), \d+\);", main_cpp)
    assert match is not None
    table = re.search(
        rf"static constexpr uint8_t {match.group(1)}\[\] PROGMEM = (\{{[^;]*\}});",
        main_cpp,
    )
    assert table is not None
    return table.group(1)


def test_swreset_and_reset_delay_without_reset_pin(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """A model with no reset_pin gets SWRESET plus the ST7701S 50ms delay."""
    yaml_file = tmp_path / "no_reset.yaml"
    yaml_file.write_text(_NO_RESET_PIN_YAML)

    main_cpp = generate_main(yaml_file)

    assert _init_sequence(main_cpp, "no_reset_display").startswith("{1, 0, 50, 255,")


def test_reset_delay_only_with_reset_pin(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
) -> None:
    """A model with a reset_pin default skips SWRESET but keeps the settling delay."""
    yaml_file = tmp_path / "has_reset.yaml"
    yaml_file.write_text(_RESET_PIN_YAML)

    main_cpp = generate_main(yaml_file)

    assert _init_sequence(main_cpp, "has_reset_display").startswith("{50, 255,")
