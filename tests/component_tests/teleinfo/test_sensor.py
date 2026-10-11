"""Tests for teleinfo sensor tag presets."""

import pytest

from esphome.components.teleinfo import CONF_TAG_NAME
from esphome.components.teleinfo.sensor import apply_tag_config
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ICON,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_REACTIVE_ENERGY,
    DEVICE_CLASS_VOLTAGE,
    ICON_FLASH,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_KILOVOLT_AMPS,
    UNIT_KILOWATT_HOURS,
    UNIT_MILLIAMP,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_VOLT_AMPS_REACTIVE_HOURS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)


@pytest.mark.parametrize(
    ("tag", "unit", "device_class", "state_class"),
    [
        ("EAST", UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING),
        (
            "ERQ1",
            UNIT_VOLT_AMPS_REACTIVE_HOURS,
            DEVICE_CLASS_REACTIVE_ENERGY,
            STATE_CLASS_TOTAL_INCREASING,
        ),
        ("IRMS1", UNIT_AMPERE, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT),
        ("URMS1", UNIT_VOLT, DEVICE_CLASS_VOLTAGE, STATE_CLASS_MEASUREMENT),
        (
            "SINSTS",
            UNIT_VOLT_AMPS,
            DEVICE_CLASS_APPARENT_POWER,
            STATE_CLASS_MEASUREMENT,
        ),
        (
            "PREF",
            UNIT_KILOVOLT_AMPS,
            DEVICE_CLASS_APPARENT_POWER,
            STATE_CLASS_MEASUREMENT,
        ),
        ("CCASN", UNIT_WATT, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
        ("HCHC", UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING),
        ("BBRHCJB", UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY, STATE_CLASS_TOTAL_INCREASING),
        ("IINST1", UNIT_AMPERE, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT),
        ("ADIR2", UNIT_AMPERE, DEVICE_CLASS_CURRENT, STATE_CLASS_MEASUREMENT),
        ("PAPP", UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER, STATE_CLASS_MEASUREMENT),
        ("PMAX", UNIT_WATT, DEVICE_CLASS_POWER, STATE_CLASS_MEASUREMENT),
    ],
)
def test_known_tag_preset(
    tag: str, unit: str, device_class: str, state_class: str
) -> None:
    config = apply_tag_config({CONF_TAG_NAME: tag})
    assert config[CONF_UNIT_OF_MEASUREMENT] == unit
    assert config[CONF_DEVICE_CLASS] == device_class
    assert config[CONF_STATE_CLASS] == state_class
    assert CONF_ICON not in config


def test_unknown_tag_keeps_legacy_defaults() -> None:
    config = apply_tag_config({CONF_TAG_NAME: "MYTAG"})
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_WATT_HOURS
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_ENERGY
    assert config[CONF_STATE_CLASS] == STATE_CLASS_TOTAL_INCREASING
    assert config[CONF_ICON] == ICON_FLASH


def test_unknown_tag_user_unit_keeps_legacy_device_class() -> None:
    config = apply_tag_config(
        {CONF_TAG_NAME: "MYTAG", CONF_UNIT_OF_MEASUREMENT: UNIT_KILOWATT_HOURS}
    )
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_KILOWATT_HOURS
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_ENERGY
    assert config[CONF_STATE_CLASS] == STATE_CLASS_TOTAL_INCREASING
    assert config[CONF_ICON] == ICON_FLASH


def test_unknown_tag_starting_with_u_is_not_voltage() -> None:
    config = apply_tag_config({CONF_TAG_NAME: "UNKNOWN"})
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_ENERGY


def test_numeric_tag_name_does_not_crash() -> None:
    config = apply_tag_config({CONF_TAG_NAME: 1234})
    assert config[CONF_TAG_NAME] == 1234
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_ENERGY


@pytest.mark.parametrize(
    ("tag", "unit", "device_class"),
    [
        ("HCHP", UNIT_KILOWATT_HOURS, DEVICE_CLASS_ENERGY),
        ("EAST", UNIT_KILOWATT_HOURS, DEVICE_CLASS_ENERGY),
        ("PREF", UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER),
        ("IINST1", UNIT_MILLIAMP, DEVICE_CLASS_CURRENT),
    ],
)
def test_user_compatible_unit_keeps_preset_device_class(
    tag: str, unit: str, device_class: str
) -> None:
    config = apply_tag_config({CONF_TAG_NAME: tag, CONF_UNIT_OF_MEASUREMENT: unit})
    assert config[CONF_UNIT_OF_MEASUREMENT] == unit
    assert config[CONF_DEVICE_CLASS] == device_class


def test_user_incompatible_unit_skips_preset_device_class() -> None:
    config = apply_tag_config({CONF_TAG_NAME: "PAPP", CONF_UNIT_OF_MEASUREMENT: "W"})
    assert config[CONF_UNIT_OF_MEASUREMENT] == "W"
    assert CONF_DEVICE_CLASS not in config
    assert config[CONF_STATE_CLASS] == STATE_CLASS_MEASUREMENT


def test_user_incompatible_device_class_skips_preset_unit() -> None:
    config = apply_tag_config(
        {CONF_TAG_NAME: "PAPP", CONF_DEVICE_CLASS: DEVICE_CLASS_POWER}
    )
    assert config[CONF_DEVICE_CLASS] == DEVICE_CLASS_POWER
    assert CONF_UNIT_OF_MEASUREMENT not in config


def test_user_same_device_class_keeps_preset_unit() -> None:
    config = apply_tag_config(
        {CONF_TAG_NAME: "PAPP", CONF_DEVICE_CLASS: DEVICE_CLASS_APPARENT_POWER}
    )
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_VOLT_AMPS


def test_user_state_class_is_kept() -> None:
    config = apply_tag_config(
        {CONF_TAG_NAME: "HCHP", CONF_STATE_CLASS: STATE_CLASS_MEASUREMENT}
    )
    assert config[CONF_STATE_CLASS] == STATE_CLASS_MEASUREMENT
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_WATT_HOURS
