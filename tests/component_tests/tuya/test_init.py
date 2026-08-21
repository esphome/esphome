"""Tests for Tuya time synchronization configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.tuya import CONF_FORCE_TIME_SYNC, CONFIG_SCHEMA
from esphome.const import CONF_TIME_ID


def test_force_time_sync_defaults_to_false() -> None:
    config = CONFIG_SCHEMA({})

    assert config[CONF_FORCE_TIME_SYNC] is False


def test_force_time_sync_accepts_time_id() -> None:
    config = CONFIG_SCHEMA(
        {
            CONF_FORCE_TIME_SYNC: True,
            CONF_TIME_ID: "tuya_time",
        }
    )

    assert config[CONF_FORCE_TIME_SYNC] is True


def test_force_time_sync_requires_time_id() -> None:
    with pytest.raises(cv.Invalid, match="force_time_sync requires time_id"):
        CONFIG_SCHEMA({CONF_FORCE_TIME_SYNC: True})
