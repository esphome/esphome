"""Tests for ble_client config validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.ble_client import (
    CONF_DESCRIPTOR_UUID,
    validate_descriptor_not_notify,
)
from esphome.const import CONF_NOTIFY
from esphome.types import ConfigType


def test_notify_with_descriptor_uuid_rejected() -> None:
    config: ConfigType = {CONF_NOTIFY: True, CONF_DESCRIPTOR_UUID: "2902"}
    with pytest.raises(cv.Invalid, match="cannot send notifications"):
        validate_descriptor_not_notify(config)


def test_descriptor_uuid_without_notify_allowed() -> None:
    config: ConfigType = {CONF_NOTIFY: False, CONF_DESCRIPTOR_UUID: "2902"}
    assert validate_descriptor_not_notify(config) is config


def test_notify_without_descriptor_uuid_allowed() -> None:
    config: ConfigType = {CONF_NOTIFY: True}
    assert validate_descriptor_not_notify(config) is config
