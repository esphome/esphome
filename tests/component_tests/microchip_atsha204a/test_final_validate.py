"""Tests for the ATSHA204A single-wire UART wiring requirements."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

RMT_CONFIG = """
esphome:
  name: atsha204a-test
esp32:
  variant: esp32
  framework:
    type: esp-idf
uart:
  id: chip_rx
  rx_pin: {rx_pin}
  baud_rate: 230400
  data_bits: 7
  rx_buffer_size: 1024
{uart_extra}
microchip_atsha204a:
  type: swi_rmt
  uart_id: chip_rx
"""

SWI_UART_CONFIG = """
esphome:
  name: atsha204a-test
{platform}
{bus}:
  id: chip_uart
  baud_rate: 230400
  data_bits: 7
  rx_buffer_size: 1024
{wiring}
microchip_atsha204a:
  type: swi_uart
  uart_id: chip_uart
  wake_pin:
    number: GPIO12
    mode:
      output: true
      open_drain: true
"""


def _validation_errors(yaml: str, tmp_path: Path) -> list[str]:
    path = tmp_path / "atsha204a.yaml"
    path.write_text(yaml, encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    return [str(err) for err in result.errors]


@pytest.mark.parametrize(
    ("rx_pin", "uart_extra", "error"),
    [
        ("GPIO21", "", None),
        ("GPIO21", "  tx_pin: GPIO22", "requires an RX-only UART"),
        ("GPIO34", "", "Use a non-inverted, output-capable GPIO"),
        (
            "{number: GPIO21, inverted: true}",
            "",
            "Use a non-inverted, output-capable GPIO",
        ),
        ("GPIO16", "", "Use an RX pin other than GPIO3, GPIO9 or GPIO16"),
    ],
)
def test_rmt_uart_wiring(
    rx_pin: str, uart_extra: str, error: str | None, tmp_path: Path
) -> None:
    errors = _validation_errors(
        RMT_CONFIG.format(rx_pin=rx_pin, uart_extra=uart_extra), tmp_path
    )
    if error is None:
        assert errors == []
    else:
        assert any(error in err for err in errors), errors


@pytest.mark.parametrize(
    ("platform", "bus", "wiring", "error"),
    [
        (
            "esp32:\n  variant: esp32",
            "uart",
            "  tx_pin: GPIO25\n  rx_pin: GPIO26",
            None,
        ),
        (
            "esp32:\n  variant: esp32",
            "uart",
            "  tx_pin: {number: GPIO25, inverted: true}\n  rx_pin: GPIO26",
            "requires non-inverted pins",
        ),
        (
            "esp32:\n  variant: esp32",
            "uart",
            "  tx_pin: GPIO25\n  rx_pin: {number: GPIO26, inverted: true}",
            "requires non-inverted pins",
        ),
        (
            "esp32:\n  variant: esp32",
            "uart",
            "  tx_pin: GPIO25\n  rx_pin: GPIO26\n  flow_control_pin: GPIO32",
            "does not support flow control",
        ),
        ("esp32:\n  variant: esp32", "virtual_uart", "", "requires a hardware UART"),
        (
            "esp8266:\n  board: d1_mini",
            "uart",
            "  tx_pin: GPIO1\n  rx_pin: GPIO3",
            None,
        ),
        (
            "esp8266:\n  board: d1_mini\nlogger:",
            "uart",
            "  tx_pin: GPIO15\n  rx_pin: GPIO13",
            "Disable or move serial logging",
        ),
        (
            "esp8266:\n  board: d1_mini\nlogger:\n  baud_rate: 0",
            "uart",
            "  tx_pin: GPIO15\n  rx_pin: GPIO13",
            None,
        ),
        (
            "esp8266:\n  board: d1_mini",
            "uart",
            "  tx_pin: GPIO4\n  rx_pin: GPIO5",
            "use hardware UART0 pins",
        ),
    ],
)
def test_swi_uart_wiring(
    platform: str, bus: str, wiring: str, error: str | None, tmp_path: Path
) -> None:
    errors = _validation_errors(
        SWI_UART_CONFIG.format(platform=platform, bus=bus, wiring=wiring), tmp_path
    )
    if error is None:
        assert errors == []
    else:
        assert any(error in err for err in errors), errors
