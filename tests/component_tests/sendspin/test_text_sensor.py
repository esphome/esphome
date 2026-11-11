"""Validation tests for the sendspin text sensor platform.

These cover behavior a compile test cannot observe: which roles a sensor type requests.
"""

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.sendspin import _get_data
from esphome.components.sendspin.text_sensor import (
    CONF_PAIRING_CODE,
    CONFIG_SCHEMA,
    SENDSPIN_TEXT_METADATA_TYPES,
)
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _sensor_config(**overrides: Any) -> ConfigType:
    """Build a minimal valid text sensor config, allowing field overrides."""
    config: ConfigType = {"name": "Sendspin Text", "type": "title"}
    config.update(overrides)
    return config


@pytest.mark.parametrize("sensor_type", sorted(SENDSPIN_TEXT_METADATA_TYPES))
def test_metadata_types_request_metadata_role(
    set_core_config: SetCoreConfigCallable, sensor_type: str
) -> None:
    """Metadata sensors need the metadata role, and say nothing about pairing codes."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_sensor_config(type=sensor_type))

    assert _get_data().metadata_support is True
    assert _get_data().pairing_code_display_support is False


def test_pairing_code_requests_code_display(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A pairing_code sensor is a way to show the dynamic code, so it alone makes the
    hub offer it. It needs no role, so a pairing-only config builds without metadata."""
    set_core_config(PlatformFramework.ESP32_IDF)

    CONFIG_SCHEMA(_sensor_config(type=CONF_PAIRING_CODE))

    assert _get_data().pairing_code_display_support is True
    assert _get_data().metadata_support is False


def test_unknown_type_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """A misspelled type must fail rather than fall through to a default."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA(_sensor_config(type="pairing_secret"))
