"""Tests for the mcu_watchdog action reset configuration validation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.tuya import (
    CONF_MCU_WATCHDOG,
    CONF_MISSED_HEARTBEATS,
    CONF_RESET_TIME,
    CONFIG_SCHEMA,
)
from esphome.const import CONF_ACTION, CONF_HEARTBEAT
from esphome.core import CORE

from .test_init import validation_errors

RESET_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
bk72xx:
  board: generic-bk7231n-qfn32-tuya
uart:
  id: uart_tuya
  tx_pin: TX1
  rx_pin: RX1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""

RESET_RP2040_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
rp2040:
  board: rpipicow
uart:
  id: uart_tuya
  tx_pin: GPIO0
  rx_pin: GPIO1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""

RESET_USB_UART_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
esp32:
  variant: esp32s3
  framework:
    type: esp-idf
usb_uart:
  - type: ch340
    channels:
      - id: uart_tuya
        baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
"""


def test_reset_defaults() -> None:
    watchdog = CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "reset"}})[
        CONF_MCU_WATCHDOG
    ]

    assert watchdog[CONF_MISSED_HEARTBEATS] == 6
    assert watchdog[CONF_RESET_TIME].total_milliseconds == 500


@pytest.mark.parametrize("value", ["49ms", "5001ms"])
def test_reset_time_range(value: str) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(
            {CONF_MCU_WATCHDOG: {CONF_ACTION: "reset", CONF_RESET_TIME: value}}
        )


@pytest.mark.parametrize(
    "watchdog",
    [
        {CONF_RESET_TIME: "500ms"},
        {CONF_ACTION: "reboot", CONF_RESET_TIME: "500ms"},
        {CONF_ACTION: "disable", CONF_RESET_TIME: "500ms"},
    ],
)
def test_reset_time_is_rejected_with_another_action(
    watchdog: dict[str, object],
) -> None:
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: watchdog})


def test_reset_requires_heartbeat() -> None:
    with pytest.raises(cv.Invalid, match="mcu_watchdog requires heartbeat"):
        CONFIG_SCHEMA(
            {CONF_HEARTBEAT: False, CONF_MCU_WATCHDOG: {CONF_ACTION: "reset"}}
        )


@pytest.mark.parametrize("yaml", [RESET_CONFIG, RESET_RP2040_CONFIG])
def test_reset_accepts_uart_with_tx_pin(yaml: str, tmp_path: Path) -> None:
    assert validation_errors(yaml, tmp_path) == []


@pytest.mark.parametrize(("pin", "name"), [("tx_pin", "TX1"), ("rx_pin", "RX1")])
def test_reset_requires_both_uart_pins(pin: str, name: str, tmp_path: Path) -> None:
    yaml = RESET_CONFIG.replace(f"  {pin}: {name}\n", "")
    assert pin not in yaml
    errors = validation_errors(yaml, tmp_path)
    assert any(f"requires a UART bus with a {pin}" in err for err in errors), errors


def test_reset_rejects_inverted_uart_tx_pin(tmp_path: Path) -> None:
    yaml = RESET_CONFIG.replace(
        "  tx_pin: TX1\n", "  tx_pin:\n    number: TX1\n    inverted: true\n"
    )
    assert "    inverted: true" in yaml
    errors = validation_errors(yaml, tmp_path)
    assert any("that is not inverted" in err for err in errors), errors


def test_reset_rejects_non_native_uart(tmp_path: Path) -> None:
    errors = validation_errors(RESET_USB_UART_CONFIG, tmp_path)
    assert any("UART bus of the microcontroller itself" in err for err in errors), (
        errors
    )


def test_reset_compiles_in_the_uart_break(
    tmp_path: Path, generate_main: Callable[[str | Path], str]
) -> None:
    path = tmp_path / "tuya.yaml"
    path.write_text(RESET_CONFIG, encoding="utf-8")
    main_cpp = generate_main(path)

    assert "->set_mcu_watchdog_reset(500);" in main_cpp
    defines = {d.name for d in CORE.defines}
    assert "USE_UART_TX_BREAK" in defines
    assert "USE_TUYA_MCU_WATCHDOG_RESET" in defines
