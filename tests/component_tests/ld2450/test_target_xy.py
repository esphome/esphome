"""Tests for the ld2450 target xy text sensor schema."""

from __future__ import annotations

import pytest

from esphome.components.ld2450.text_sensor import (
    CONF_XY,
    CONFIG_SCHEMA as TEXT_SENSOR_CONFIG_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_THROTTLE, PlatformFramework
from esphome.core import TimePeriod
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _target_1(**xy: object) -> ConfigType:
    return {"target_1": {CONF_XY: {CONF_NAME: "Target-1 XY", **xy}}}


def test_xy_throttle_defaults_to_one_second(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Like the default timeout and throttle filters of the target X and Y sensors."""
    set_core_config(PlatformFramework.ESP32_IDF)

    config = TEXT_SENSOR_CONFIG_SCHEMA(_target_1())

    assert config["target_1"][CONF_XY][CONF_THROTTLE] == TimePeriod(milliseconds=1000)


@pytest.mark.parametrize(
    ("throttle", "milliseconds"), [("0s", 0), ("200ms", 200), ("5s", 5000)]
)
def test_xy_throttle_is_configurable(
    set_core_config: SetCoreConfigCallable, throttle: str, milliseconds: int
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = TEXT_SENSOR_CONFIG_SCHEMA(_target_1(**{CONF_THROTTLE: throttle}))

    assert config["target_1"][CONF_XY][CONF_THROTTLE] == TimePeriod(
        milliseconds=milliseconds
    )


def test_xy_rejects_a_negative_throttle(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid):
        TEXT_SENSOR_CONFIG_SCHEMA(_target_1(**{CONF_THROTTLE: "-1s"}))
