"""Tests for the ATSHA204A single-wire UART wiring requirements."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

I2C_CONFIG = """
esphome:
  name: atsha204a-test
esp32:
  variant: esp32
i2c:
  id: main_bus
  sda: GPIO21
  scl: GPIO22
  frequency: {frequency}
{mux}
microchip_atsha204a:
  type: i2c
  i2c_id: {bus}
"""

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
    number: {wake_pin}
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
    ("frequency", "valid"),
    [("1kHz", False), ("10kHz", True), ("100kHz", True), ("400kHz", False)],
)
def test_direct_i2c_frequency(frequency: str, valid: bool, tmp_path: Path) -> None:
    errors = _validation_errors(
        I2C_CONFIG.format(frequency=frequency, bus="main_bus", mux=""), tmp_path
    )
    if valid:
        assert errors == []
    else:
        assert any(
            "requires a frequency between 10kHz and 100kHz" in err for err in errors
        ), errors


@pytest.mark.parametrize("frequency_override", ["", "      frequency: 50kHz"])
def test_i2c_requires_direct_bus(frequency_override: str, tmp_path: Path) -> None:
    mux = f"""\
tca9548a:
  i2c_id: main_bus
  channels:
    - bus_id: mux_bus
      channel: 0
{frequency_override}
"""
    errors = _validation_errors(
        I2C_CONFIG.format(frequency="50kHz", bus="mux_bus", mux=mux), tmp_path
    )
    assert any("requires a top-level I²C bus" in err for err in errors), errors


@pytest.mark.parametrize(
    ("rx_pin", "uart_extra", "error"),
    [
        ("GPIO21", "", None),
        ("GPIO21", "  rx_timeout: 0", "value must be at least 1"),
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
            "  tx_pin: GPIO25\n  rx_pin: GPIO26\n  rx_timeout: 0",
            "value must be at least 1",
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
        SWI_UART_CONFIG.format(
            platform=platform, bus=bus, wiring=wiring, wake_pin="GPIO12"
        ),
        tmp_path,
    )
    if error is None:
        assert errors == []
    else:
        assert any(error in err for err in errors), errors


def test_swi_uart_accepts_libretiny_with_default_yaml_buffer(tmp_path: Path) -> None:
    errors = _validation_errors(
        SWI_UART_CONFIG.format(
            platform="bk72xx:\n  board: cb3s",
            bus="uart",
            wiring="  tx_pin: P11\n  rx_pin: P10",
            wake_pin="P6",
        ).replace("rx_buffer_size: 1024", "rx_buffer_size: 256"),
        tmp_path,
    )
    assert errors == []


def test_swi_uart_rejects_ln882x_without_seven_bit_framing(tmp_path: Path) -> None:
    errors = _validation_errors(
        SWI_UART_CONFIG.format(
            platform="ln882x:\n  board: generic-ln882h",
            bus="uart",
            wiring="  tx_pin: PA02\n  rx_pin: PA03",
            wake_pin="PA01",
        ),
        tmp_path,
    )
    assert any("requires seven data bits" in err for err in errors), errors


def test_swi_uart_rejects_libretiny_without_buffer_override_support(
    tmp_path: Path,
) -> None:
    errors = _validation_errors(
        SWI_UART_CONFIG.format(
            platform="bk72xx:\n  board: cb3s\n  framework:\n    version: 1.7.0",
            bus="uart",
            wiring="  tx_pin: P11\n  rx_pin: P10",
            wake_pin="P6",
        ),
        tmp_path,
    )
    assert any("requires LibreTiny 1.8.0 or later" in err for err in errors), errors
