"""Tests for the battery_gauge sensor platform configuration schema."""

from __future__ import annotations

import itertools
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.battery_gauge import CONF_BATTERY_GAUGE_ID
from esphome.components.battery_gauge.sensor import CONFIG_SCHEMA, TYPE_STATE_OF_CHARGE
from esphome.const import (
    CONF_NAME,
    CONF_TYPE,
    DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

_counter = itertools.count()


def _config(**overrides: Any) -> dict[str, Any]:
    """Build a minimal valid battery_gauge sensor config with a unique id."""
    n = next(_counter)
    config: dict[str, Any] = {
        "id": f"bg_sensor_{n}",
        CONF_NAME: f"Battery {n}",
        CONF_BATTERY_GAUGE_ID: "my_gauge",
    }
    config.update(overrides)
    return config


def test_minimal_config_defaults_to_state_of_charge() -> None:
    """With no type: key, state_of_charge is selected and default sensor metadata applies."""
    result = CONFIG_SCHEMA(_config())

    assert result[CONF_TYPE] == TYPE_STATE_OF_CHARGE
    assert result["unit_of_measurement"] == UNIT_PERCENT
    assert result["device_class"] == DEVICE_CLASS_BATTERY
    assert result["state_class"] == STATE_CLASS_MEASUREMENT
    assert result["accuracy_decimals"] == 1


def test_explicit_type_state_of_charge_is_valid() -> None:
    result = CONFIG_SCHEMA(_config(**{CONF_TYPE: TYPE_STATE_OF_CHARGE}))
    assert result[CONF_TYPE] == TYPE_STATE_OF_CHARGE


def test_unknown_type_raises() -> None:
    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_config(**{CONF_TYPE: "time_to_full"}))


def test_battery_gauge_id_is_optional() -> None:
    """With no battery_gauge_id: key, an ID is auto-generated for later resolution."""
    config = _config()
    del config[CONF_BATTERY_GAUGE_ID]
    result = CONFIG_SCHEMA(config)
    assert CONF_BATTERY_GAUGE_ID in result
