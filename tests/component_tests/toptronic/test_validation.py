"""Tests for the toptronic datapoint validation."""

import pytest

from esphome.components.toptronic import (
    CONF_DATAPOINT,
    CONF_DEVICE_TYPE,
    CONF_FUNCTION_GROUP,
    CONF_FUNCTION_NUMBER,
    CONF_TOPTRONIC_ID,
    CONF_VALUES,
    _final_validate,
    number as tt_number,
    select as tt_select,
    text_sensor as tt_text_sensor,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_OPTIONS, CONF_TYPE, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB = "tt_hub"
COORDS = {
    CONF_TOPTRONIC_ID: HUB,
    CONF_FUNCTION_GROUP: 50,
    CONF_FUNCTION_NUMBER: 0,
    CONF_DATAPOINT: 40652,
}
HUB_CONFIG = {CONF_ID: ID(HUB), CONF_DEVICE_TYPE: "HV"}


def _entity(**extra: object) -> ConfigType:
    """A hand-written entity config as it appears in the validated config tree."""
    return {"platform": "toptronic", **COORDS, **extra}


def test_two_read_entities_on_one_datapoint_are_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The second entity would never be polled, so it has to fail at config time."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"sensor": [_entity(name="A"), _entity(name="B")]},
    )
    with pytest.raises(cv.Invalid, match="already read"):
        _final_validate(HUB_CONFIG)


def test_number_and_sensor_on_one_datapoint_are_allowed(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A write entity takes its state from the read entity, so sharing the key is the normal case."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"sensor": [_entity()], "number": [_entity()]},
    )
    _final_validate(HUB_CONFIG)


def test_select_paired_with_a_numeric_sensor_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The hub would cast the select to a number, which is undefined behaviour."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"sensor": [_entity()], "select": [_entity()]},
    )
    with pytest.raises(cv.Invalid, match="can only take its state from a text_sensor"):
        _final_validate(HUB_CONFIG)


def test_preset_entity_counts_as_the_read_entity(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """HV ships a preset sensor on 50/0/40651; a second sensor on that key is a duplicate."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"sensor": [_entity(datapoint=40651)]},
    )
    with pytest.raises(cv.Invalid, match="preset sensor"):
        _final_validate(HUB_CONFIG)


def test_select_value_must_fit_the_type() -> None:
    """S8 cannot carry 200, so the option would silently write 127 instead."""
    with pytest.raises(cv.Invalid, match="does not fit type S8"):
        tt_select.CONFIG_SCHEMA(
            {
                "name": "Choice",
                **COORDS,
                CONF_OPTIONS: ["Off", "On"],
                CONF_VALUES: [0, 200],
                CONF_TYPE: "S8",
            }
        )


def test_select_value_within_the_type_is_accepted() -> None:
    config = tt_select.CONFIG_SCHEMA(
        {
            "name": "Choice",
            **COORDS,
            CONF_OPTIONS: ["Low", "High"],
            CONF_VALUES: [0, 127],
            CONF_TYPE: "S8",
        }
    )
    assert config[CONF_VALUES] == [0, 127]


def test_options_and_values_must_have_the_same_length() -> None:
    """A shorter 'values' would index out of range during code generation."""
    with pytest.raises(cv.Invalid, match="must match 'values' length"):
        tt_select.CONFIG_SCHEMA(
            {
                "name": "Choice",
                **COORDS,
                CONF_OPTIONS: ["Off", "On"],
                CONF_VALUES: [0],
            }
        )


def test_text_sensor_options_and_values_must_have_the_same_length() -> None:
    with pytest.raises(cv.Invalid, match="must match 'values' length"):
        tt_text_sensor.CONFIG_SCHEMA(
            {
                "name": "State",
                **COORDS,
                CONF_OPTIONS: ["Off"],
                CONF_VALUES: [0, 1],
            }
        )


def test_number_decimal_is_bounded() -> None:
    """A large decimal overflows the float multiplier to infinity."""
    with pytest.raises(cv.Invalid):
        tt_number.CONFIG_SCHEMA(
            {
                "name": "Setpoint",
                **COORDS,
                CONF_TYPE: "S16",
                "min_value": 100,
                "max_value": 300,
                "step": 5,
                "decimal": 7,
            }
        )


def test_number_range_and_step_are_raw_units() -> None:
    """min_value/max_value/step stay in raw units and are only divided for the entity."""
    config = tt_number.CONFIG_SCHEMA(
        {
            "name": "Setpoint",
            **COORDS,
            CONF_TYPE: "S16",
            "min_value": 100,
            "max_value": 300,
            "step": 5,
            "decimal": 1,
        }
    )
    assert config["min_value"] == 100
    assert config["max_value"] == 300
    assert config["step"] == 5


NUMBER = {"type": "U16", "min_value": 0, "max_value": 100, "step": 1}


def test_two_writers_on_one_datapoint_are_rejected(set_core_config) -> None:
    """The hub keys inputs by datapoint, so the second writer would be dropped."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"number": [_entity(**NUMBER), _entity(**NUMBER)]},
    )
    with pytest.raises(cv.Invalid, match="already written"):
        _final_validate(HUB_CONFIG)


def test_linked_number_and_sensor_must_share_the_wire_type(set_core_config) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={
            "sensor": [_entity(type="U16")],
            "number": [_entity(**{**NUMBER, "type": "S16"})],
        },
    )
    with pytest.raises(cv.Invalid, match="wire types must match"):
        _final_validate(HUB_CONFIG)


def test_linked_select_and_text_sensor_must_share_the_mapping(set_core_config) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={
            "text_sensor": [_entity(options=["Off", "On"], values=[0, 1])],
            "select": [_entity(options=["Low", "High"], values=[0, 1])],
        },
    )
    with pytest.raises(cv.Invalid, match="mapping differs"):
        _final_validate(HUB_CONFIG)


def test_number_bounds_must_fit_the_wire_type() -> None:
    with pytest.raises(cv.Invalid, match="must fit type U8"):
        tt_number.CONFIG_SCHEMA(
            {
                "name": "Setpoint",
                **COORDS,
                **NUMBER,
                CONF_TYPE: "U8",
                "max_value": 1000,
            }
        )


def test_number_step_must_be_positive() -> None:
    with pytest.raises(cv.Invalid):
        tt_number.CONFIG_SCHEMA({"name": "Setpoint", **COORDS, **NUMBER, "step": 0})


def test_duplicate_option_values_are_rejected() -> None:
    with pytest.raises(cv.Invalid, match="'values' must be unique"):
        tt_select.CONFIG_SCHEMA(
            {
                "name": "Choice",
                **COORDS,
                CONF_OPTIONS: ["Off", "On"],
                CONF_VALUES: [1, 1],
            }
        )


def test_duplicate_select_labels_are_rejected() -> None:
    with pytest.raises(cv.Invalid, match="must be unique for a select"):
        tt_select.CONFIG_SCHEMA(
            {
                "name": "Choice",
                **COORDS,
                CONF_OPTIONS: ["Off", "Off"],
                CONF_VALUES: [0, 1],
            }
        )
