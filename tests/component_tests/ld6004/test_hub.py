"""Validate the LD6004 hub and binary sensor configuration limits."""

import pytest

from esphome.components.ld6004 import CONFIG_SCHEMA, LD6004Component
from esphome.components.ld6004.binary_sensor import (
    CONFIG_SCHEMA as BINARY_SENSOR_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def test_minimal_hub(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config: ConfigType = CONFIG_SCHEMA({})
    assert config[CONF_ID].type == LD6004Component


@pytest.mark.parametrize(
    ("key", "value"),
    [("out_pin", 4), ("stale_timeout", "1s"), ("wakeup_pin", 4)],
)
def test_unsupported_hub_option(
    set_core_config: SetCoreConfigCallable, key: str, value: int | str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match=key):
        CONFIG_SCHEMA({key: value})


def test_target_3(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    config: ConfigType = BINARY_SENSOR_SCHEMA({"target_3": {"name": "Target 3"}})
    assert config["target_3"]["name"] == "Target 3"


def test_target_4_rejected(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    with pytest.raises(cv.Invalid, match="target_4"):
        BINARY_SENSOR_SCHEMA({"target_4": {"name": "Target 4"}})
