"""Tests for the climate visual temperature conversion."""

import pytest

from esphome.components.climate import (
    _visual_temperatures_in_entity_unit,
    visual_temperature_step,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_TARGET_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    UNIT_CELSIUS,
    UNIT_FAHRENHEIT,
)
from esphome.core.entity_helpers import entity_temperature_unit


def test_visual_temperature_step_scalar_applies_to_both() -> None:
    step = visual_temperature_step("1°F")
    assert step == {
        CONF_TARGET_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
        CONF_CURRENT_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
    }


def test_visual_temperatures_default_to_celsius() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (50.0, UNIT_FAHRENHEIT),
                CONF_MAX_TEMPERATURE: (30.0, None),
                CONF_TEMPERATURE_STEP: visual_temperature_step("1.8°F"),
            }
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == pytest.approx(10.0)
    assert visual[CONF_MAX_TEMPERATURE] == 30.0
    assert visual[CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == pytest.approx(1.0)


def test_visual_temperatures_follow_unit_of_measurement() -> None:
    config = _visual_temperatures_in_entity_unit(
        {
            CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT),
            CONF_VISUAL: {
                CONF_MIN_TEMPERATURE: (60.0, None),
                CONF_MAX_TEMPERATURE: (30.0, UNIT_CELSIUS),
                CONF_TEMPERATURE_STEP: {
                    CONF_TARGET_TEMPERATURE: (1.0, UNIT_FAHRENHEIT),
                    CONF_CURRENT_TEMPERATURE: (0.5, UNIT_CELSIUS),
                },
            },
        }
    )
    visual = config[CONF_VISUAL]
    assert visual[CONF_MIN_TEMPERATURE] == 60.0
    assert visual[CONF_MAX_TEMPERATURE] == pytest.approx(86.0)
    assert visual[CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == 1.0
    assert visual[CONF_TEMPERATURE_STEP][CONF_CURRENT_TEMPERATURE] == pytest.approx(0.9)


def test_only_a_validated_unit_is_the_entity_unit() -> None:
    # anova validates its own unit_of_measurement for the device; it is not the entity unit
    assert entity_temperature_unit({CONF_UNIT_OF_MEASUREMENT: "f"}) == UNIT_CELSIUS
    assert (
        entity_temperature_unit({CONF_UNIT_OF_MEASUREMENT: UNIT_FAHRENHEIT})
        == UNIT_CELSIUS
    )
    assert (
        entity_temperature_unit(
            {CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT)}
        )
        == UNIT_FAHRENHEIT
    )


def test_visual_temperatures_converted_once() -> None:
    config = {CONF_VISUAL: {CONF_MIN_TEMPERATURE: (50.0, UNIT_FAHRENHEIT)}}
    _visual_temperatures_in_entity_unit(config)
    _visual_temperatures_in_entity_unit(config)
    assert config[CONF_VISUAL][CONF_MIN_TEMPERATURE] == pytest.approx(10.0)
