"""Tests for validation of the nRF52 hardware UART."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

NRF52_UART_CONFIG = """
esphome:
  name: nrf52-uart
nrf52:
  board: adafruit_feather_nrf52840
uart:
  rx_pin: P1.09
  tx_pin: P1.08
  baud_rate: 9600
  rx_buffer_size: {rx_buffer_size}
"""

MCUMGR_OTA = """
ota:
  - platform: zephyr_mcumgr
    transport:
      hardware_uart: {port}
"""


def _errors(text: str, tmp_path: Path) -> list[str]:
    path = tmp_path / "nrf52_uart.yaml"
    path.write_text(text, encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    return [str(err) for err in result.errors]


@pytest.mark.parametrize(("rx_buffer_size", "valid"), [(0, False), (1, True)])
def test_rx_buffer_size_above_zero(
    rx_buffer_size: int, valid: bool, tmp_path: Path
) -> None:
    errors = _errors(NRF52_UART_CONFIG.format(rx_buffer_size=rx_buffer_size), tmp_path)
    if valid:
        assert errors == []
    else:
        assert any("rx_buffer_size above 0" in err for err in errors), errors


@pytest.mark.parametrize(("port", "valid"), [("UART1", False), ("CDC", True)])
def test_mcumgr_ota_on_uart1(port: str, valid: bool, tmp_path: Path) -> None:
    errors = _errors(
        NRF52_UART_CONFIG.format(rx_buffer_size=256) + MCUMGR_OTA.format(port=port),
        tmp_path,
    )
    if valid:
        assert errors == []
    else:
        assert any("zephyr_mcumgr OTA also uses" in err for err in errors), errors
