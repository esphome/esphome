"""Tests for the battery_gauge sensor configuration schema."""

from __future__ import annotations

import itertools
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.battery_gauge.sensor import (
    CONF_CURRENT_SOURCE,
    CONF_EFFICIENCY,
    CONF_MAX_CHARGE_VOLTAGE,
    CONF_VOLTAGE_SOURCE,
    CONFIG_SCHEMA,
    capacity_ah,
)
from esphome.const import (
    CONF_CAPACITY,
    CONF_INITIAL_STATE,
    DEVICE_CLASS_BATTERY,
    ICON_BATTERY,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
)

# A unique counter so that every generated config has a distinct name/id.
# The duplicate-entity-name registry persists for the duration of a single
# test, so reusing a name within one test would raise a spurious error.
_counter = itertools.count()


def _config(**overrides: Any) -> dict[str, Any]:
    """Build a minimal valid battery_gauge sensor config with unique name/id."""
    n = next(_counter)
    config: dict[str, Any] = {
        "id": f"bg_{n}",
        "name": f"Battery {n}",
        CONF_VOLTAGE_SOURCE: "voltage_sensor",
        CONF_CURRENT_SOURCE: "current_sensor",
        CONF_CAPACITY: "5Ah",
        CONF_MAX_CHARGE_VOLTAGE: "4.2V",
    }
    config.update(overrides)
    return config


# --- minimal / full config ---


def test_minimal_config_is_valid() -> None:
    """A config with only the required keys validates and applies defaults."""
    result = CONFIG_SCHEMA(_config())

    assert result[CONF_CAPACITY] == 5.0
    assert result[CONF_MAX_CHARGE_VOLTAGE] == 4.2
    # default efficiency
    assert result[CONF_EFFICIENCY] == 0.98
    # initial_state is optional and has no default
    assert CONF_INITIAL_STATE not in result
    # sensor schema metadata
    assert result["unit_of_measurement"] == UNIT_PERCENT
    assert result["device_class"] == DEVICE_CLASS_BATTERY
    assert result["state_class"] == STATE_CLASS_MEASUREMENT
    assert result["accuracy_decimals"] == 1
    assert result["icon"] == ICON_BATTERY


def test_full_config_is_valid() -> None:
    """All optional keys are carried through validation."""
    result = CONFIG_SCHEMA(
        _config(
            **{
                CONF_EFFICIENCY: 0.9,
                CONF_INITIAL_STATE: 0.5,
            }
        )
    )

    assert result[CONF_EFFICIENCY] == 0.9
    assert result[CONF_INITIAL_STATE] == 0.5


# --- required keys ---


@pytest.mark.parametrize(
    "missing_key",
    [
        CONF_VOLTAGE_SOURCE,
        CONF_CURRENT_SOURCE,
        CONF_CAPACITY,
        CONF_MAX_CHARGE_VOLTAGE,
    ],
)
def test_required_key_missing_raises(missing_key: str) -> None:
    """Omitting any required key is a validation error."""
    config = _config()
    del config[missing_key]
    with pytest.raises(cv.Invalid, match=rf"required key not provided.*{missing_key}"):
        CONFIG_SCHEMA(config)


# --- capacity parsing ---


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("5Ah", 5.0),
        ("2.5ah", 2.5),
        ("10AH", 10.0),
        ("3aH", 3.0),
        ("7", 7.0),
        (4.2, 4.2),
    ],
)
def test_capacity_units(value: Any, expected: float) -> None:
    """capacity_ah accepts an optional Ah suffix in any case."""
    assert capacity_ah(value) == expected


def test_capacity_bad_suffix_raises() -> None:
    """An unrecognised capacity suffix is rejected."""
    with pytest.raises(cv.Invalid, match="capacity suffix"):
        capacity_ah("5xyz")


# --- initial_state range ---


@pytest.mark.parametrize("value", [0.0, 0.5, 1.0, "50%"])
def test_initial_state_valid(value: Any) -> None:
    """initial_state accepts the inclusive range 0..1 (and percent strings)."""
    result = CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: value}))
    assert 0.0 <= result[CONF_INITIAL_STATE] <= 1.0


def test_initial_state_above_one_raises() -> None:
    """A bare number above 1.0 requires a percent sign and is rejected."""
    with pytest.raises(cv.Invalid, match="percent sign"):
        CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: 1.5}))


def test_initial_state_negative_raises() -> None:
    """initial_state below 0 is out of range."""
    with pytest.raises(cv.Invalid, match="at least 0"):
        CONFIG_SCHEMA(_config(**{CONF_INITIAL_STATE: -0.1}))
