"""Tests for emontx sensor tag defaults."""

import pytest

from esphome.components import sensor
from esphome.components.emontx.sensor import BASE_SCHEMA, apply_tag_defaults
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_STATE_CLASS,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
)


def _schema_default(schema, key_name):
    """Return the default registered for *key_name* in *schema*.

    Returns ``cv.UNDEFINED`` when the key is absent or has no default.
    Voluptuous stores defaults on the ``Optional`` marker object; the marker's
    ``.schema`` attribute holds the plain string key.
    """
    for k in schema.schema:
        if hasattr(k, "schema") and k.schema == key_name:
            return getattr(k, "default", cv.UNDEFINED)
    return cv.UNDEFINED


def test_base_schema_has_no_state_class_default():
    """BASE_SCHEMA must not pre-populate state_class.

    sensor_schema(state_class=...) registers the key via
    cv.Optional(key, default=...), making it always present in the validated
    config dict.  apply_tag_defaults then skips it (``key not in config``),
    so every sensor ends up with the schema-level fallback instead of its
    prefix-specific value.  This test catches that regression directly.
    """
    assert _schema_default(BASE_SCHEMA, CONF_STATE_CLASS) is cv.UNDEFINED


def test_base_schema_has_no_accuracy_decimals_default():
    """BASE_SCHEMA must not pre-populate accuracy_decimals.

    Same root cause as the state_class regression: a default registered in
    sensor_schema() prevents apply_tag_defaults from injecting the correct
    per-prefix value (e.g. 2 for voltage/current/temperature sensors).
    """
    assert _schema_default(BASE_SCHEMA, CONF_ACCURACY_DECIMALS) is cv.UNDEFINED


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
