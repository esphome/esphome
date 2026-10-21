"""Tests for the climate visual temperature conversion."""

import pytest

from esphome.components.climate import (
    Climate,
    _visual_temperatures_in_entity_unit,
    climate_schema,
    visual_temperature_step,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_CURRENT_TEMPERATURE,
    CONF_ID,
    CONF_MAX_TEMPERATURE,
    CONF_MIN_TEMPERATURE,
    CONF_NAME,
    CONF_TARGET_TEMPERATURE,
    CONF_TEMPERATURE_STEP,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_VISUAL,
    UNIT_CELSIUS,
    UNIT_FAHRENHEIT,
    UNIT_KELVIN,
)
from esphome.core import CORE, ID, entity_helpers
from esphome.core.entity_helpers import (
    entity_temperature_unit,
    entity_temperature_unit_of_id,
    setup_entity_temperature_unit,
    temperature_in_entity_unit,
)
from esphome.cpp_generator import MockObj


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


def test_a_fixed_unit_keeps_visual_temperatures_in_it() -> None:
    # friedrich reports in °F through its traits; the unit is stamped, not a user option
    schema = climate_schema(Climate, fixed_temperature_unit=UNIT_FAHRENHEIT)
    config = schema({CONF_NAME: "AC", CONF_VISUAL: {CONF_TEMPERATURE_STEP: "1°F"}})
    unit = config[CONF_UNIT_OF_MEASUREMENT]
    assert unit == UNIT_FAHRENHEIT
    assert isinstance(unit, cv.FixedTemperatureUnitValue)
    assert config[CONF_VISUAL][CONF_TEMPERATURE_STEP][CONF_TARGET_TEMPERATURE] == 1.0
    with pytest.raises(cv.Invalid, match="unit_of_measurement"):
        schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_FAHRENHEIT})


def test_a_fixed_unit_is_not_passed_to_the_platform() -> None:
    calls: list[str] = []
    original_add = entity_helpers.add
    entity_helpers.add = lambda expression: calls.append(str(expression))
    try:
        var = MockObj("ac")
        setup_entity_temperature_unit(
            var, {CONF_UNIT_OF_MEASUREMENT: cv.FixedTemperatureUnitValue("°F")}
        )
        assert calls == []
        setup_entity_temperature_unit(
            var, {CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit("°F")}
        )
        assert len(calls) == 1
        assert calls[0].startswith("ac.set_temperature_unit(")
        assert calls[0].endswith("TemperatureUnit::FAHRENHEIT)")
    finally:
        entity_helpers.add = original_add


def test_kelvin_is_rejected_with_mqtt() -> None:
    schema = climate_schema(Climate, temperature_unit=True)
    CORE.loaded_integrations.add("mqtt")
    try:
        with pytest.raises(cv.Invalid, match="°C or °F"):
            schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_KELVIN})
    finally:
        CORE.loaded_integrations.discard("mqtt")
    config = schema({CONF_NAME: "AC", CONF_UNIT_OF_MEASUREMENT: UNIT_KELVIN})
    assert config[CONF_UNIT_OF_MEASUREMENT] == UNIT_KELVIN


def test_action_temperatures_follow_the_target_entity_unit() -> None:
    ac = ID("ac", is_declaration=True, type="climate::Climate")
    full = Config()
    full["climate"] = [
        {CONF_ID: ac, CONF_UNIT_OF_MEASUREMENT: cv.temperature_unit(UNIT_FAHRENHEIT)}
    ]
    full.declare_ids.append((ac, ["climate", 0, CONF_ID]))
    CORE.config = full

    target = ID("ac", is_declaration=False)
    assert entity_temperature_unit_of_id(target) == UNIT_FAHRENHEIT
    assert temperature_in_entity_unit(
        {CONF_ID: target}, (20.0, UNIT_CELSIUS)
    ).startswith("68.0")
