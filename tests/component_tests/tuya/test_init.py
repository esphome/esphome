"""Tests for Tuya time synchronization configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.tuya import CONF_FORCE_TIME_SYNC, CONFIG_SCHEMA
from esphome.const import CONF_TIME_ID, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


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


def test_protocol_defaults_to_standard() -> None:
    config = CONFIG_SCHEMA({})

    assert config["protocol"] == "standard"


def _esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )


def test_low_power_accepts_its_options(set_core_config: SetCoreConfigCallable) -> None:
    _esp32(set_core_config)
    config = CONFIG_SCHEMA(
        {
            "protocol": "low_power",
            "mcu_wake_pin": {"number": 12, "inverted": True},
            "on_mcu_ready": [{"delay": "1s"}],
        }
    )

    assert config["protocol"] == "low_power"
    assert config["mcu_wake_pin"]["inverted"] is True
    assert len(config["on_mcu_ready"]) == 1


@pytest.mark.parametrize(
    ("options", "message"),
    [
        (
            {"protocol": "low_power", CONF_FORCE_TIME_SYNC: True, CONF_TIME_ID: "t"},
            "force_time_sync is unsupported for low_power",
        ),
        (
            {"protocol": "low_power", "status_pin": 14},
            "status_pin is unsupported for low_power",
        ),
        ({"mcu_wake_pin": 12}, "mcu_wake_pin requires protocol: low_power"),
        (
            {"on_mcu_ready": [{"delay": "1s"}]},
            "on_mcu_ready requires protocol: low_power",
        ),
    ],
)
def test_low_power_options_rejected(
    set_core_config: SetCoreConfigCallable, options: dict, message: str
) -> None:
    _esp32(set_core_config)
    with pytest.raises(cv.Invalid, match=message):
        CONFIG_SCHEMA(options)
