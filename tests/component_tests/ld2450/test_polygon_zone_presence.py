"""Tests for the ld2450 polygon zone presence binary sensor schema."""

from __future__ import annotations

import pytest

from esphome.components.ld2450 import CONF_POLYGON_ZONE_ID, CONF_POLYGON_ZONES
from esphome.components.ld2450.binary_sensor import (
    CONFIG_SCHEMA as BINARY_SENSOR_CONFIG_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import CONF_FILTERS, CONF_NAME, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _presence(**extra: object) -> ConfigType:
    return {
        CONF_POLYGON_ZONES: [
            {CONF_POLYGON_ZONE_ID: "couch_zone", CONF_NAME: "Couch Occupied", **extra}
        ]
    }


def test_presence_without_filters_passes(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    cv.Schema(BINARY_SENSOR_CONFIG_SCHEMA)(_presence())


def test_presence_with_filters_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Clearing a polygon bypasses filters, so they would get out of step with the zone."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(
        cv.Invalid,
        match=r"^Polygon zone presence binary sensors can't have filters; .*presence_timeout",
    ):
        cv.Schema(BINARY_SENSOR_CONFIG_SCHEMA)(
            _presence(**{CONF_FILTERS: [{"delayed_off": "1s"}]})
        )
