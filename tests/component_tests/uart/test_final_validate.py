"""Tests for final validation of UART device requirements against usb_uart channels."""

from pathlib import Path

import pytest

from esphome import config, yaml_util
from esphome.core import CORE

USB_UART_CONFIG = """
esphome:
  name: usb-uart-final-validate
esp32:
  variant: esp32s3
  framework:
    type: esp-idf
usb_uart:
  - type: ch340
    channels:
      - id: meter_uart
        baud_rate: 1200
        stop_bits: {stop_bits}
sensor:
  - platform: kamstrup_kmp
    uart_id: meter_uart
    heat_energy:
      name: Heat energy
"""


@pytest.mark.parametrize(
    ("stop_bits", "valid"), [("2", True), ("1", False), ("1.5", False)]
)
def test_stop_bits_requirement_on_usb_uart_channel(
    stop_bits: str, valid: bool, tmp_path: Path
) -> None:
    """usb_uart stores stop bits as strings; they must still satisfy a numeric requirement."""
    path = tmp_path / "usb_uart.yaml"
    path.write_text(USB_UART_CONFIG.format(stop_bits=stop_bits), encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    errors = [str(err) for err in result.errors]
    if valid:
        assert errors == []
    else:
        assert any("requires 2 stop bits" in err for err in errors), errors
