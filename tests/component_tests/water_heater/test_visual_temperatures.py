"""Tests for the water heater visual temperature conversion."""

import pytest

from esphome.components.water_heater import (
    CONF_TARGET_TEMPERATURE_STEP,
    _visual_temperatures_in_entity_unit,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    UNIT_CELSIUS,
    UNIT_FAHRENHEIT,
)


def test_visual_temperatures_default_to_celsius() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (100.0, UNIT_FAHRENHEIT),
                CONF_MAX_TEMPERATURE: (85.0, None),
                CONF_TARGET_TEMPERATURE_STEP: (1.8, UNIT_FAHRENHEIT),
            }
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == pytest.approx(37.78, abs=0.01)
    assert visual[CONF_MAX_TEMPERATURE] == 85.0
    assert visual[CONF_TARGET_TEMPERATURE_STEP] == pytest.approx(1.0)


def test_visual_temperatures_follow_unit_of_measurement() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT),
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (100.0, None),
                CONF_MAX_TEMPERATURE: (60.0, UNIT_CELSIUS),
                CONF_TARGET_TEMPERATURE_STEP: (1.0, UNIT_CELSIUS),
            },
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == 100.0
    assert visual[CONF_MAX_TEMPERATURE] == pytest.approx(140.0)
    assert visual[CONF_TARGET_TEMPERATURE_STEP] == pytest.approx(1.8)
