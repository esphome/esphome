"""Tests for template cover config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.template.cover import (
    CONF_STOP_TILT_ACTION,
    _validate_stop_tilt_action,
)
from esphome.const import CONF_TILT_ACTION
from esphome.types import ConfigType


def test_stop_tilt_action_with_tilt_action_allowed() -> None:
    config: ConfigType = {CONF_TILT_ACTION: [{}], CONF_STOP_TILT_ACTION: [{}]}
    assert _validate_stop_tilt_action(config) is config


def test_stop_tilt_action_without_tilt_action_rejected() -> None:
    config: ConfigType = {CONF_STOP_TILT_ACTION: [{}]}
    with pytest.raises(cv.Invalid, match="requires 'tilt_action'"):
        _validate_stop_tilt_action(config)
