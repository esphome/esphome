"""Tests for the grundfos_alpha config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.grundfos_alpha import CONFIG_SCHEMA, validate_update_interval
from esphome.const import CONF_UPDATE_INTERVAL


@pytest.mark.parametrize("value", ["5s", "30s", "10min"])
def test_update_interval_of_5_s_or_more_is_accepted(value: str) -> None:
    assert validate_update_interval(value) == cv.update_interval(value)


def test_update_interval_never_is_accepted() -> None:
    assert validate_update_interval("never") == cv.update_interval("never")


@pytest.mark.parametrize("value", ["4s", "1s", "500ms"])
def test_update_interval_below_5_s_is_rejected(value: str) -> None:
    with pytest.raises(cv.Invalid):
        validate_update_interval(value)


def test_schema_uses_the_checked_update_interval() -> None:
    keys = [k for k in CONFIG_SCHEMA.schema if k == CONF_UPDATE_INTERVAL]
    assert len(keys) == 1
    assert keys[0].default() == "30s"
    assert CONFIG_SCHEMA.schema[keys[0]] is validate_update_interval
