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
    CONF_STATE_CLASS,
    CONF_TAG,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_EMPTY,
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
    config = apply_tag_defaults(_config("P", **{CONF_DEVICE_CLASS: DEVICE_CLASS_EMPTY}))
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_EMPTY
    # Other defaults for the same tag are still applied.
    assert config[CONF_UNIT_OF_MEASUREMENT] == POWER_CONFIG[CONF_UNIT_OF_MEASUREMENT]
    assert config[CONF_ACCURACY_DECIMALS] == POWER_CONFIG[CONF_ACCURACY_DECIMALS]
