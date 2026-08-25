"""Tests for mk2pvrouter sensor tag-based default injection."""

import pytest

from esphome.components.mk2pvrouter.sensor import (
    DIVERSION_RATE_CONFIG,
    ENERGY_CONFIG,
    POWER_CONFIG,
    RELAY_STATE_CONFIG,
    TEMPERATURE_CONFIG,
    VOLTAGE_CONFIG,
    apply_tag_defaults,
)
from esphome.components.sensor import validate_state_class
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_FILTERS,
    CONF_MULTIPLY,
    CONF_STATE_CLASS,
    CONF_TAG,
    CONF_UNIT_OF_MEASUREMENT,
    STATE_CLASS_MEASUREMENT,
)
from esphome.types import ConfigType


def _config(tag: str, **extra) -> ConfigType:
    return {CONF_TAG: tag, **extra}


@pytest.mark.parametrize(
    ("tag", "expected"),
    [
        ("P", POWER_CONFIG),
        ("D", POWER_CONFIG),
        ("V", VOLTAGE_CONFIG),
        ("E", ENERGY_CONFIG),
        ("R", POWER_CONFIG),
    ],
)
def test_exact_tag_match(tag: str, expected: dict) -> None:
    config = apply_tag_defaults(_config(tag))
    assert config[CONF_UNIT_OF_MEASUREMENT] == expected[CONF_UNIT_OF_MEASUREMENT]
    assert config[CONF_DEVICE_CLASS] == expected[CONF_DEVICE_CLASS]
    assert config[CONF_ACCURACY_DECIMALS] == expected[CONF_ACCURACY_DECIMALS]


@pytest.mark.parametrize(
    ("tag", "expected"),
    [
        ("P1", POWER_CONFIG),
        ("P2", POWER_CONFIG),
        ("D1", DIVERSION_RATE_CONFIG),
        ("V1", VOLTAGE_CONFIG),
        ("T1", TEMPERATURE_CONFIG),
        ("R1", RELAY_STATE_CONFIG),
        ("R10", RELAY_STATE_CONFIG),
    ],
)
def test_pattern_tag_match(tag: str, expected: dict) -> None:
    config = apply_tag_defaults(_config(tag))
    assert config[CONF_UNIT_OF_MEASUREMENT] == expected[CONF_UNIT_OF_MEASUREMENT]
    assert config[CONF_DEVICE_CLASS] == expected[CONF_DEVICE_CLASS]


def test_lowercase_tag_is_matched_case_insensitively() -> None:
    config = apply_tag_defaults(_config("v1"))
    assert config[CONF_UNIT_OF_MEASUREMENT] == VOLTAGE_CONFIG[CONF_UNIT_OF_MEASUREMENT]


@pytest.mark.parametrize("tag", ["S_MC", "STATUS", "X9", "Z"])
def test_unknown_tag_falls_back_to_defaults(tag: str) -> None:
    config = apply_tag_defaults(_config(tag))
    assert config[CONF_ACCURACY_DECIMALS] == 0
    assert config[CONF_STATE_CLASS] == validate_state_class(STATE_CLASS_MEASUREMENT)
    assert CONF_UNIT_OF_MEASUREMENT not in config
    assert CONF_DEVICE_CLASS not in config


def test_user_supplied_value_is_not_overridden() -> None:
    config = apply_tag_defaults(_config("P", **{CONF_UNIT_OF_MEASUREMENT: "Wh"}))
    assert config[CONF_UNIT_OF_MEASUREMENT] == "Wh"
    # Other defaults for the same tag are still applied.
    assert config[CONF_DEVICE_CLASS] == POWER_CONFIG[CONF_DEVICE_CLASS]


def test_builtin_filters_are_prepended_not_replaced() -> None:
    config = apply_tag_defaults(
        _config("V1", **{CONF_FILTERS: [{CONF_MULTIPLY: 0.001}]})
    )
    multiply_values = [f[CONF_MULTIPLY] for f in config[CONF_FILTERS]]
    assert multiply_values == [0.01, 0.001]


def test_no_user_filters_still_gets_builtin_filters() -> None:
    config = apply_tag_defaults(_config("T1"))
    assert len(config[CONF_FILTERS]) == 1
    assert config[CONF_FILTERS][0][CONF_MULTIPLY] == 0.01


def test_tag_without_builtin_filters_has_none_added() -> None:
    config = apply_tag_defaults(_config("P1"))
    assert CONF_FILTERS not in config
