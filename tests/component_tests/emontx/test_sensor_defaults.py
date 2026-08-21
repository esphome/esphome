"""Tests for emontx sensor tag defaults."""

import pytest

from esphome.components import sensor
from esphome.components.emontx.sensor import CONFIG_SCHEMA, apply_tag_defaults
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_STATE_CLASS,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
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
        # Unknown / free-form tags fall back to generic defaults
        ("CUSTOM1", STATE_CLASS_MEASUREMENT, 0),
        ("X", STATE_CLASS_MEASUREMENT, 0),
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
