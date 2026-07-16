"""Tests for emontx sensor tag defaults."""

import pytest

from esphome.components import sensor
from esphome.components.emontx.sensor import apply_tag_defaults
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_STATE_CLASS,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)


def _make_config(tag: str) -> dict:
    """Minimal config dict with only tag_name set — no overrides."""
    return {"tag_name": tag}


def _state_class_str(config: dict) -> str:
    """Return the state_class value as a plain string for easy assertion."""
    return sensor.validate_state_class.schema.schema[config[CONF_STATE_CLASS]]


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
