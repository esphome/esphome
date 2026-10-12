"""Adjustable TMP102 thermostat thresholds."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_INITIAL_VALUE,
    CONF_RESTORE_VALUE,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_CELSIUS,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    CONF_EXTENDED_MODE,
    CONF_TEMPERATURE_HIGH,
    CONF_TEMPERATURE_LOW,
    CONF_TMP102_ID,
    TMP102_DEFAULT_THIGH,
    TMP102_DEFAULT_TLOW,
    TMP102Component,
    TMP102LimitType,
    get_number_configuration,
    set_number_configuration,
    tmp102_ns,
    validate_tmp102_thresholds,
    validate_unique_child,
)

TMP102LimitNumber = tmp102_ns.class_("TMP102LimitNumber", number.Number, cg.Component)

_LIMIT_TYPES = {
    CONF_TEMPERATURE_LOW: TMP102LimitType.TMP102_LIMIT_LOW,
    CONF_TEMPERATURE_HIGH: TMP102LimitType.TMP102_LIMIT_HIGH,
}

_LIMIT_SCHEMA = (
    number.number_schema(
        TMP102LimitNumber,
        unit_of_measurement=UNIT_CELSIUS,
        device_class=DEVICE_CLASS_TEMPERATURE,
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
    .extend(
        {
            cv.Optional(CONF_INITIAL_VALUE): cv.temperature,
            cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_TMP102_ID): cv.use_id(TMP102Component),
            cv.Optional(CONF_TEMPERATURE_HIGH): _LIMIT_SCHEMA,
            cv.Optional(CONF_TEMPERATURE_LOW): _LIMIT_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(CONF_TEMPERATURE_HIGH, CONF_TEMPERATURE_LOW),
)


def _final_validate(config: ConfigType) -> ConfigType:
    validate_unique_child(config, "number")
    full = fv.full_config.get()
    path = full.get_path_for_id(config[CONF_TMP102_ID])[:-1]
    parent = full.get_config_for_path(path)
    extended = parent.get(CONF_EXTENDED_MODE, False)
    merged = dict(parent)
    resolved = {}

    for key, default in (
        (CONF_TEMPERATURE_HIGH, TMP102_DEFAULT_THIGH),
        (CONF_TEMPERATURE_LOW, TMP102_DEFAULT_TLOW),
    ):
        value = parent.get(key, default)
        if key in config:
            limit_config = config[key]
            if CONF_INITIAL_VALUE in limit_config:
                if key in parent:
                    raise cv.Invalid(
                        f"Set {key} on the sensor or initial_value on its number, not both"
                    )
                value = limit_config[CONF_INITIAL_VALUE]
            merged[key] = value
        resolved[key] = value

    validate_tmp102_thresholds(merged)
    set_number_configuration(
        config[CONF_TMP102_ID],
        extended,
        resolved[CONF_TEMPERATURE_HIGH],
        resolved[CONF_TEMPERATURE_LOW],
    )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_NUMBER")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    number_config = get_number_configuration(config[CONF_TMP102_ID])
    assert number_config is not None
    extended = number_config[CONF_EXTENDED_MODE]
    for key, setter in (
        (CONF_TEMPERATURE_HIGH, parent.set_high_limit_control),
        (CONF_TEMPERATURE_LOW, parent.set_low_limit_control),
    ):
        if limit_config := config.get(key):
            var = await number.new_number(
                limit_config,
                parent,
                _LIMIT_TYPES[key],
                min_value=-55.0,
                max_value=150.0 if extended else 127.9375,
                step=0.0625,
            )
            await cg.register_component(var, limit_config)
            cg.add(var.set_initial_value(number_config[key]))
            cg.add(var.set_restore_value(limit_config[CONF_RESTORE_VALUE]))
            cg.add(setter(var))
