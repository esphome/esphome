"""Tests for Tuya configuration validation."""

from pathlib import Path

import pytest

from esphome import config, config_validation as cv, yaml_util
from esphome.components.tuya import (
    CONF_DATAPOINT_ACK,
    CONF_FORCE_TIME_SYNC,
    CONF_MCU_WATCHDOG,
    CONF_MISSED_HEARTBEATS,
    CONF_RESET_TIME,
    CONFIG_SCHEMA,
)
from esphome.const import CONF_ACTION, CONF_HEARTBEAT, CONF_TIME_ID
from esphome.core import CORE


def test_force_time_sync_accepts_time_id() -> None:
    config = CONFIG_SCHEMA(
        {
            CONF_FORCE_TIME_SYNC: True,
            CONF_TIME_ID: "tuya_time",
        }
    )

    assert config[CONF_FORCE_TIME_SYNC] is True


def test_force_time_sync_requires_time_id() -> None:
    with pytest.raises(cv.Invalid, match="force_time_sync requires time_id"):
        CONFIG_SCHEMA({CONF_FORCE_TIME_SYNC: True})


@pytest.mark.parametrize("value", [0, 21])
def test_mcu_watchdog_missed_heartbeats_range(value: int) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_MISSED_HEARTBEATS: value}})


def test_mcu_watchdog_is_on_by_default() -> None:
    assert CONFIG_SCHEMA({})[CONF_MCU_WATCHDOG][CONF_ACTION] == "reboot"


def test_mcu_watchdog_can_be_disabled() -> None:
    config = CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "disable"}})

    assert config[CONF_MCU_WATCHDOG][CONF_ACTION] == "disable"


BK72XX_HEADER = """
esphome:
  name: tuya-mcu-watchdog
bk72xx:
  board: generic-bk7231n-qfn32-tuya
"""

WATCHDOG_CONFIG = (
    BK72XX_HEADER
    + """
uart:
  id: uart_tuya
  tx_pin:
    number: TX1
    allow_other_uses: true
  rx_pin: RX1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
    tx_pin:
      number: {tx_pin}
      allow_other_uses: true
"""
)

WATCHDOG_USB_UART_CONFIG = """
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
    tx_pin: GPIO4
"""

WATCHDOG_RP2040_CONFIG = """
esphome:
  name: tuya-mcu-watchdog
rp2040:
  board: rpipicow
uart:
  id: uart_tuya
  tx_pin:
    number: GPIO0
    allow_other_uses: true
  rx_pin: GPIO1
  baud_rate: 9600
tuya:
  uart_id: uart_tuya
  mcu_watchdog:
    action: reset
    tx_pin:
      number: GPIO0
      allow_other_uses: true
"""


WATCHDOG_DEFAULT_CONFIG = """
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
  mcu_watchdog: {}
"""


def _validation_errors(yaml: str, tmp_path: Path) -> list[str]:
    path = tmp_path / "tuya.yaml"
    path.write_text(yaml, encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    return [str(err) for err in result.errors]


def test_mcu_watchdog_accepts_uart_tx_pin(tmp_path: Path) -> None:
    assert _validation_errors(WATCHDOG_CONFIG.format(tx_pin="TX1"), tmp_path) == []


def test_mcu_watchdog_rejects_other_pin(tmp_path: Path) -> None:
    errors = _validation_errors(WATCHDOG_CONFIG.format(tx_pin="P6"), tmp_path)
    assert any("must be the tx_pin of the UART bus" in err for err in errors), errors


def test_mcu_watchdog_rejects_non_native_uart(tmp_path: Path) -> None:
    errors = _validation_errors(WATCHDOG_USB_UART_CONFIG, tmp_path)
    assert any("UART bus of the microcontroller itself" in err for err in errors), (
        errors
    )


def test_mcu_watchdog_rejects_unsupported_platform(tmp_path: Path) -> None:
    errors = _validation_errors(WATCHDOG_RP2040_CONFIG, tmp_path)
    assert any("only available on" in err for err in errors), errors


def test_mcu_watchdog_default_is_reboot_on_any_platform(tmp_path: Path) -> None:
    assert _validation_errors(WATCHDOG_DEFAULT_CONFIG, tmp_path) == []
    assert CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {}})[CONF_MCU_WATCHDOG] == {
        CONF_ACTION: "reboot",
        CONF_MISSED_HEARTBEATS: 6,
        CONF_RESET_TIME: cv.TimePeriod(milliseconds=500),
    }


def test_mcu_watchdog_reset_requires_tx_pin(tmp_path: Path) -> None:
    errors = _validation_errors(
        WATCHDOG_CONFIG.split("    tx_pin:", maxsplit=1)[0], tmp_path
    )
    assert any("action: reset requires tx_pin" in err for err in errors), errors


def test_mcu_watchdog_rejects_inverted_tx_pin(tmp_path: Path) -> None:
    yaml = WATCHDOG_CONFIG.format(tx_pin="TX1") + "      inverted: true" + chr(10)
    errors = _validation_errors(yaml, tmp_path)
    assert any("tx_pin must not be inverted" in err for err in errors), errors


def test_heartbeat_and_datapoint_ack_defaults() -> None:
    config = CONFIG_SCHEMA({})

    assert config[CONF_HEARTBEAT] is True
    assert config[CONF_DATAPOINT_ACK] is False


def test_heartbeat_can_be_disabled() -> None:
    config = CONFIG_SCHEMA({CONF_HEARTBEAT: False, CONF_DATAPOINT_ACK: True})

    assert config[CONF_HEARTBEAT] is False
    assert config[CONF_DATAPOINT_ACK] is True


@pytest.mark.parametrize("value", ["49ms", "5001ms"])
def test_mcu_watchdog_reset_time_range(value: str) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_RESET_TIME: value}})


def test_mcu_watchdog_rejects_unknown_action() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "restart"}})


def test_mcu_watchdog_reboot_requires_heartbeat() -> None:
    with pytest.raises(cv.Invalid, match="mcu_watchdog requires heartbeat"):
        CONFIG_SCHEMA({CONF_HEARTBEAT: False, CONF_MCU_WATCHDOG: {}})


def test_heartbeat_off_accepts_disabled_watchdog() -> None:
    config = CONFIG_SCHEMA(
        {CONF_HEARTBEAT: False, CONF_MCU_WATCHDOG: {CONF_ACTION: "disable"}}
    )

    assert config[CONF_HEARTBEAT] is False


def test_mcu_watchdog_rejects_inverted_uart_tx_pin(tmp_path: Path) -> None:
    yaml = WATCHDOG_CONFIG.format(tx_pin="TX1").replace(
        "    number: TX1"
        + chr(10)
        + "    allow_other_uses: true"
        + chr(10)
        + "  rx_pin",
        "    number: TX1"
        + chr(10)
        + "    allow_other_uses: true"
        + chr(10)
        + "    inverted: true"
        + chr(10)
        + "  rx_pin",
    )
    assert "    inverted: true" in yaml
    errors = _validation_errors(yaml, tmp_path)
    assert any("that is not inverted" in err for err in errors), errors


def test_mcu_watchdog_reset_requires_heartbeat(tmp_path: Path) -> None:
    yaml = WATCHDOG_CONFIG.format(tx_pin="TX1") + "  heartbeat: false" + chr(10)
    errors = _validation_errors(yaml, tmp_path)
    assert any("mcu_watchdog requires heartbeat" in err for err in errors), errors
