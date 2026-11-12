"""Tests for which ESP8266 pin pairs enable the Arduino hardware Serial objects."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

CONFIG = """
esphome:
  name: test
esp8266:
  board: d1_mini
logger:
  baud_rate: 0
uart:
  baud_rate: 9600
{pins}
"""


@pytest.mark.parametrize(
    ("pins", "serial", "serial1"),
    [
        ("  tx_pin: GPIO1\n  rx_pin: GPIO3", True, False),
        ("  tx_pin: GPIO15\n  rx_pin: GPIO13", True, False),
        ("  rx_pin: GPIO13", True, False),
        ("  tx_pin: GPIO2", False, True),
        # Mixed UART0 pins: setup() picks software serial
        ("  tx_pin: GPIO1\n  rx_pin: GPIO13", False, False),
        ("  tx_pin: GPIO5\n  rx_pin: GPIO4", False, False),
    ],
)
def test_hardware_serial_defines_follow_setup_pin_pairs(
    generate_main: Callable[[str | Path], str],
    tmp_path: Path,
    pins: str,
    serial: bool,
    serial1: bool,
) -> None:
    path = tmp_path / "test.yaml"
    path.write_text(CONFIG.format(pins=pins), encoding="utf-8")
    generate_main(path)

    defines = {d.name for d in CORE.defines}
    assert ("USE_ESP8266_UART_SERIAL" in defines) is serial
    assert ("USE_ESP8266_UART_SERIAL1" in defines) is serial1
