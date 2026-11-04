"""Tests for template cover config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.template.cover import CONF_STOP_TILT_ACTION, CONFIG_SCHEMA
from esphome.const import CONF_ID, CONF_NAME, CONF_TILT_ACTION
from esphome.types import ConfigType


def _cover_config(**extra: object) -> ConfigType:
    return {CONF_ID: "test_cover", CONF_NAME: "Test Cover", **extra}


def test_stop_tilt_action_with_tilt_action_allowed() -> None:
    config = CONFIG_SCHEMA(
        _cover_config(**{CONF_TILT_ACTION: [], CONF_STOP_TILT_ACTION: []})
    )
    assert CONF_STOP_TILT_ACTION in config


def test_stop_tilt_action_without_tilt_action_rejected() -> None:
    with pytest.raises(cv.Invalid, match="requires 'tilt_action'") as exc_info:
        CONFIG_SCHEMA(_cover_config(**{CONF_STOP_TILT_ACTION: []}))
    assert exc_info.value.path == [CONF_STOP_TILT_ACTION]
