"""Tests for emontx sensor tag defaults."""

import pytest

from esphome.components import sensor
from esphome.components.emontx.sensor import CONFIG_SCHEMA, apply_tag_defaults
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEVICE_CLASS,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    DEVICE_CLASS_APPARENT_POWER,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_POWER_FACTOR,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_VOLTAGE,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_AMPERE,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_HERTZ,
    UNIT_PULSES,
    UNIT_VOLT,
    UNIT_VOLT_AMPS,
    UNIT_WATT,
    UNIT_WATT_HOURS,
)


def _resolve_via_config_schema(tag: str) -> dict:
    """Run a minimal config through the real CONFIG_SCHEMA pipeline, the
    same path a user's YAML goes through."""
    return CONFIG_SCHEMA(
        {"tag_name": tag, "emontx_id": "my_emontx", "name": f"{tag} sensor"}
    )


def test_config_schema_applies_tag_default_state_class():
    """If sensor_schema(state_class=...) is reintroduced, the schema-level
    default wins over apply_tag_defaults' per-prefix value, and E1 would
    resolve to measurement instead of total_increasing. Driving the real
    CONFIG_SCHEMA (not just apply_tag_defaults) catches that, since
    sensor_schema() runs before apply_tag_defaults in the cv.All() chain.
    """
    result = _resolve_via_config_schema("E1")
    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(
        STATE_CLASS_TOTAL_INCREASING
    )


def test_config_schema_applies_tag_default_accuracy_decimals():
    """Same root cause as the state_class regression: reintroducing
    sensor_schema(accuracy_decimals=...) would make V1 resolve to the
    schema-level default instead of the prefix-specific value of 2.
    """
    result = _resolve_via_config_schema("V1")
    assert result[CONF_ACCURACY_DECIMALS] == 2


def _make_config(tag: str) -> dict:
    """Minimal config dict with only tag_name set — no overrides."""
    return {"tag_name": tag}


@pytest.mark.parametrize(
    ("tag", "expected_state_class", "expected_decimals"),
    [
        # Known numeric-index prefixes
        ("E1", STATE_CLASS_TOTAL_INCREASING, 0),
        ("E12", STATE_CLASS_TOTAL_INCREASING, 0),
        ("P1", STATE_CLASS_MEASUREMENT, 0),
        ("V1", STATE_CLASS_MEASUREMENT, 2),
        ("I1", STATE_CLASS_MEASUREMENT, 2),
        ("T1", STATE_CLASS_MEASUREMENT, 2),
        # Known patterns
        ("PULSE1", STATE_CLASS_TOTAL_INCREASING, 0),
        ("PULSE12", STATE_CLASS_TOTAL_INCREASING, 0),
        ("PF1", STATE_CLASS_MEASUREMENT, 2),
        ("AP1", STATE_CLASS_MEASUREMENT, 2),
        ("AP12", STATE_CLASS_MEASUREMENT, 2),
        # Frequency: reported as a single, un-numbered tag
        ("F", STATE_CLASS_MEASUREMENT, 2),
        # Unknown / free-form tags fall back to generic defaults
        ("CUSTOM1", STATE_CLASS_MEASUREMENT, 0),
        ("X", STATE_CLASS_MEASUREMENT, 0),
        # "F1" is not the exact "F" tag, so it falls back to generic defaults
        ("F1", STATE_CLASS_MEASUREMENT, 0),
        # "PULSE" (no index) is how some real emonTx firmware reports a
        # single pulse counter, so it still resolves to the PULSE defaults
        ("PULSE", STATE_CLASS_TOTAL_INCREASING, 0),
        # Real firmware sends this lowercase; tag_upper's case-folding must
        # still match it against the PULSE pattern
        ("pulse", STATE_CLASS_TOTAL_INCREASING, 0),
        # PF/AP require a numeric index; the bare prefix alone (no index)
        # falls back to generic defaults
        ("PF", STATE_CLASS_MEASUREMENT, 0),
        ("AP", STATE_CLASS_MEASUREMENT, 0),
    ],
)
def test_apply_tag_defaults(tag, expected_state_class, expected_decimals):
    """apply_tag_defaults must inject the correct state_class and accuracy_decimals
    for each tag type when no user overrides are present."""
    config = _make_config(tag)
    result = apply_tag_defaults(config)

    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(expected_state_class)
    assert result[CONF_ACCURACY_DECIMALS] == expected_decimals


@pytest.mark.parametrize(
    ("tag", "expected_unit", "expected_device_class"),
    [
        # Known numeric-index prefixes
        ("E1", UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY),
        ("E12", UNIT_WATT_HOURS, DEVICE_CLASS_ENERGY),
        ("P1", UNIT_WATT, DEVICE_CLASS_POWER),
        ("V1", UNIT_VOLT, DEVICE_CLASS_VOLTAGE),
        ("I1", UNIT_AMPERE, DEVICE_CLASS_CURRENT),
        ("T1", UNIT_CELSIUS, DEVICE_CLASS_TEMPERATURE),
        # Known patterns
        ("PULSE1", UNIT_PULSES, DEVICE_CLASS_ENERGY),
        ("PULSE12", UNIT_PULSES, DEVICE_CLASS_ENERGY),
        # Bare "PULSE" (no index), as reported by some real emonTx firmware
        ("PULSE", UNIT_PULSES, DEVICE_CLASS_ENERGY),
        # Real firmware sends this lowercase; tag_upper's case-folding must
        # still match it against the PULSE pattern
        ("pulse", UNIT_PULSES, DEVICE_CLASS_ENERGY),
        ("PF1", UNIT_EMPTY, DEVICE_CLASS_POWER_FACTOR),
        ("AP1", UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER),
        ("AP12", UNIT_VOLT_AMPS, DEVICE_CLASS_APPARENT_POWER),
        # Frequency: reported as a single, un-numbered tag
        ("F", UNIT_HERTZ, DEVICE_CLASS_FREQUENCY),
    ],
)
def test_apply_tag_defaults_unit_and_device_class(
    tag, expected_unit, expected_device_class
):
    """apply_tag_defaults must inject the correct, validated unit_of_measurement
    and device_class for each tag type when no user overrides are present."""
    config = _make_config(tag)
    result = apply_tag_defaults(config)

    assert result[CONF_UNIT_OF_MEASUREMENT] == sensor.validate_unit_of_measurement(
        expected_unit
    )
    assert result[CONF_DEVICE_CLASS] == sensor.validate_device_class(
        expected_device_class
    )


@pytest.mark.parametrize(
    "tag",
    [
        "CUSTOM1",
        "X",
        # Non-numeric suffixes must not collide with a PATTERN_CONFIGS prefix
        # (e.g. "APPLE" starting with "AP", "PFX" starting with "PF").
        "APPLE",
        "PFX",
        "PULSE_A",
        # "F1" is not the exact "F" tag
        "F1",
        # Bare "PF"/"AP" (no numeric index) don't match; unlike "PULSE",
        # real firmware never reports these without an index
        "PF",
        "AP",
    ],
)
def test_apply_tag_defaults_unknown_tag_has_no_unit_or_device_class(tag):
    """Unknown / free-form tags only get generic state_class and
    accuracy_decimals defaults; unit_of_measurement and device_class are left
    for the user to set explicitly."""
    config = _make_config(tag)
    result = apply_tag_defaults(config)

    assert CONF_UNIT_OF_MEASUREMENT not in result
    assert CONF_DEVICE_CLASS not in result
    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(
        STATE_CLASS_MEASUREMENT
    )
    assert result[CONF_ACCURACY_DECIMALS] == 0


@pytest.mark.parametrize(
    ("tag", "user_state_class", "user_decimals"),
    [
        # User overrides must not be clobbered by defaults
        ("E1", STATE_CLASS_MEASUREMENT, 3),
        ("PULSE1", STATE_CLASS_MEASUREMENT, 1),
        ("V1", STATE_CLASS_TOTAL_INCREASING, 0),
        ("CUSTOM1", STATE_CLASS_TOTAL_INCREASING, 4),
    ],
)
def test_apply_tag_defaults_respects_user_overrides(
    tag, user_state_class, user_decimals
):
    """apply_tag_defaults must not overwrite values already set by the user."""
    config = _make_config(tag)
    config[CONF_STATE_CLASS] = sensor.validate_state_class(user_state_class)
    config[CONF_ACCURACY_DECIMALS] = user_decimals

    result = apply_tag_defaults(config)

    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(user_state_class)
    assert result[CONF_ACCURACY_DECIMALS] == user_decimals
