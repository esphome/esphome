"""Tests for Tuya configuration validation."""

from pathlib import Path

import pytest

from esphome import config, config_validation as cv, yaml_util
from esphome.components.tuya import (
    AUTO_LOAD,
    CONF_DATAPOINT_ACK,
    CONF_FORCE_TIME_SYNC,
    CONF_MCU_WATCHDOG,
    CONF_MISSED_HEARTBEATS,
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


def test_auto_load_requests_uart_for_probe_and_config() -> None:
    assert AUTO_LOAD(None) == ["uart"]
    assert AUTO_LOAD({"id": "x"}) == ["uart"]


@pytest.mark.parametrize("value", [0, 21])
def test_mcu_watchdog_missed_heartbeats_range(value: int) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_MISSED_HEARTBEATS: value}})


def test_mcu_watchdog_is_off_unless_set() -> None:
    assert CONF_MCU_WATCHDOG not in CONFIG_SCHEMA({})


def test_mcu_watchdog_can_be_disabled() -> None:
    config = CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "disable"}})

    assert config[CONF_MCU_WATCHDOG][CONF_ACTION] == "disable"


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


def validation_errors(yaml: str, tmp_path: Path) -> list[str]:
    path = tmp_path / "tuya.yaml"
    path.write_text(yaml, encoding="utf-8")
    CORE.config_path = path
    result = config.validate_config(yaml_util.load_yaml(path), {})
    return [str(err) for err in result.errors]


def test_mcu_watchdog_default_is_reboot_on_any_platform(tmp_path: Path) -> None:
    assert validation_errors(WATCHDOG_DEFAULT_CONFIG, tmp_path) == []
    assert CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {}})[CONF_MCU_WATCHDOG] == {
        CONF_ACTION: "reboot",
        CONF_MISSED_HEARTBEATS: 6,
    }


def test_heartbeat_and_datapoint_ack_defaults() -> None:
    config = CONFIG_SCHEMA({})

    assert config[CONF_HEARTBEAT] is True
    assert config[CONF_DATAPOINT_ACK] is False


def test_heartbeat_can_be_disabled() -> None:
    config = CONFIG_SCHEMA({CONF_HEARTBEAT: False, CONF_DATAPOINT_ACK: True})

    assert config[CONF_HEARTBEAT] is False
    assert config[CONF_DATAPOINT_ACK] is True


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


def test_heartbeat_off_rejects_reboot_watchdog_in_a_full_config(tmp_path: Path) -> None:
    yaml = WATCHDOG_DEFAULT_CONFIG + "  heartbeat: false" + chr(10)
    errors = validation_errors(yaml, tmp_path)
    assert any("mcu_watchdog requires heartbeat" in err for err in errors), errors


def test_mcu_watchdog_disable_rejects_missed_heartbeats() -> None:
    with pytest.raises(cv.Invalid, match="extra keys not allowed"):
        CONFIG_SCHEMA(
            {CONF_MCU_WATCHDOG: {CONF_ACTION: "disable", CONF_MISSED_HEARTBEATS: 6}}
        )


def test_mcu_watchdog_action_is_case_insensitive() -> None:
    config = CONFIG_SCHEMA({CONF_MCU_WATCHDOG: {CONF_ACTION: "Disable"}})

    assert config[CONF_MCU_WATCHDOG] == {CONF_ACTION: "disable"}


@pytest.mark.parametrize("value", ["tuya", ["tuya"], 1])
def test_config_that_is_not_a_mapping_is_a_config_error(value: object) -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(value)


def test_invalid_heartbeat_is_reported_at_its_key() -> None:
    with pytest.raises(cv.Invalid) as excinfo:
        CONFIG_SCHEMA({CONF_HEARTBEAT: "maybe", CONF_MCU_WATCHDOG: {}})

    assert excinfo.value.path == [CONF_HEARTBEAT]
