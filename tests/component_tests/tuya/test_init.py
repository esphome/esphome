"""Tests for Tuya time synchronization configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.tuya import CONF_FORCE_TIME_SYNC, CONFIG_SCHEMA
from esphome.const import CONF_TIME_ID


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
    assert config["auto_baud"] is False


def test_low_power_accepts_its_options() -> None:
    config = CONFIG_SCHEMA(
        {"protocol": "low_power", "auto_baud": True, "product_id": "ihedujbuli6hsx9b"}
    )

    assert config["protocol"] == "low_power"
    assert config["product_id"] == "ihedujbuli6hsx9b"


@pytest.mark.parametrize(
    ("options", "message"),
    [
        ({"product_id": "abc"}, "product_id requires protocol: low_power"),
        ({"auto_baud": True}, "auto_baud is only supported with protocol: low_power"),
        (
            {"protocol": "low_power", CONF_FORCE_TIME_SYNC: True, CONF_TIME_ID: "t"},
            "force_time_sync is unsupported for low_power",
        ),
    ],
)
def test_low_power_options_rejected(options: dict, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        CONFIG_SCHEMA(options)
