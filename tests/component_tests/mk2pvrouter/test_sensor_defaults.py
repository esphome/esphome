"""Tests for mk2pvrouter sensor tag defaults driven through the real CONFIG_SCHEMA."""

from esphome.components import sensor
from esphome.components.mk2pvrouter.sensor import CONFIG_SCHEMA
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
        {"tag": tag, "mk2pvrouter_id": "my_mk2pvrouter", "name": f"{tag} sensor"}
    )


def test_config_schema_applies_tag_default_state_class():
    """If sensor_schema(state_class=...) is reintroduced, the schema-level
    default wins over apply_tag_defaults' per-tag value, and E would resolve
    to measurement instead of total_increasing. Driving the real
    CONFIG_SCHEMA (not just apply_tag_defaults) catches that, since
    sensor_schema() runs before apply_tag_defaults in the cv.All() chain.
    """
    result = _resolve_via_config_schema("E")
    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(
        STATE_CLASS_TOTAL_INCREASING
    )


def test_config_schema_applies_tag_default_accuracy_decimals():
    """Same root cause as the state_class regression: reintroducing
    sensor_schema(accuracy_decimals=...) would make V resolve to the
    schema-level default instead of the tag-specific value of 2.
    """
    result = _resolve_via_config_schema("V")
    assert result[CONF_ACCURACY_DECIMALS] == 2


def test_config_schema_uppercases_tag_before_defaults_are_applied():
    """The schema uppercases the tag (see MK2PVROUTER_LISTENER_SCHEMA) before
    apply_tag_defaults runs, so a lowercase tag from YAML still resolves to
    the correct defaults through the real pipeline (not just when calling
    apply_tag_defaults directly)."""
    result = _resolve_via_config_schema("v")
    assert result[CONF_STATE_CLASS] == sensor.validate_state_class(
        STATE_CLASS_MEASUREMENT
    )
    assert result[CONF_ACCURACY_DECIMALS] == 2
