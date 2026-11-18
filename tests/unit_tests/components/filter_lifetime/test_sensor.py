"""Tests for the filter_lifetime configuration schema."""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.filter_lifetime import sensor as filter_lifetime
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_DURATION,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_HOUR,
    UNIT_PERCENT,
)
from esphome.core import Lambda
from esphome.types import ConfigType


def _validate(config: dict[str, Any]) -> ConfigType:
    return filter_lifetime.CONFIG_SCHEMA({"name": "Filter", **config})


def test_max_lifetime_is_stored_in_minutes() -> None:
    config = _validate({"max_lifetime": "180d"})
    assert config["max_lifetime"].total_minutes == 180 * 24 * 60


def test_max_lifetime_is_required() -> None:
    with pytest.raises(cv.Invalid, match="required key not provided"):
        _validate({})


@pytest.mark.parametrize("value", ["0min", "30s", 6])
def test_max_lifetime_rejects_values_under_one_minute(value: Any) -> None:
    with pytest.raises(cv.Invalid):
        _validate({"max_lifetime": value})


def test_defaults() -> None:
    config = _validate({"max_lifetime": "180d"})
    assert config[CONF_UPDATE_INTERVAL].total_seconds == 60
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_PERCENT
    assert config[CONF_ACCURACY_DECIMALS] == 2
    assert "is_on" not in config
    assert "current_speed" not in config


@pytest.mark.parametrize(
    ("lambda_key", "sensor_key"),
    [("is_on", "is_on_sensor"), ("current_speed", "current_speed_sensor")],
)
def test_lambda_and_sensor_are_mutually_exclusive(
    lambda_key: str, sensor_key: str
) -> None:
    with pytest.raises(cv.Invalid, match="Cannot specify more than one of"):
        _validate(
            {
                "max_lifetime": "180d",
                lambda_key: Lambda("return 1;"),
                sensor_key: "some_id",
            }
        )


def test_lambda_inputs_accepted() -> None:
    config = _validate(
        {
            "max_lifetime": "180d",
            "is_on": Lambda("return true;"),
            "current_speed": Lambda("return 50.0f;"),
        }
    )
    assert config["is_on"].value == "return true;"
    assert config["current_speed"].value == "return 50.0f;"


def test_sub_sensor_defaults() -> None:
    config = _validate(
        {
            "max_lifetime": "180d",
            "runtime_hours": {"name": "Runtime"},
            "remaining_days": {"name": "Remaining"},
        }
    )
    runtime = config["runtime_hours"]
    assert runtime[CONF_UNIT_OF_MEASUREMENT] == UNIT_HOUR
    assert runtime[CONF_DEVICE_CLASS] == DEVICE_CLASS_DURATION
    assert runtime[CONF_STATE_CLASS] == STATE_CLASS_TOTAL_INCREASING
    remaining = config["remaining_days"]
    assert remaining[CONF_UNIT_OF_MEASUREMENT] == filter_lifetime.UNIT_DAY
    assert remaining[CONF_DEVICE_CLASS] == DEVICE_CLASS_DURATION
    assert remaining[CONF_ACCURACY_DECIMALS] == 0
