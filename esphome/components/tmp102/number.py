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
from esphome.types import ConfigType

from . import (
    CONF_EXTENDED_MODE,
    CONF_TEMPERATURE_HIGH,
    CONF_TEMPERATURE_LOW,
    CONF_TMP102_ID,
    LIMIT_TYPES,
    TMP102Component,
    get_parent_config,
    tmp102_ns,
)

TMP102LimitNumber = tmp102_ns.class_("TMP102LimitNumber", number.Number, cg.Component)

DEPENDENCIES = ["sensor"]

_KEY_INITIAL_VALUE = "_tmp102_initial_value"

LIMIT_SCHEMA = (
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
            cv.Required(CONF_TMP102_ID): cv.use_id(TMP102Component),
            cv.Optional(CONF_TEMPERATURE_HIGH): LIMIT_SCHEMA,
            cv.Optional(CONF_TEMPERATURE_LOW): LIMIT_SCHEMA,
        }
    ),
    cv.has_at_least_one_key(CONF_TEMPERATURE_HIGH, CONF_TEMPERATURE_LOW),
)


def _final_validate(config: ConfigType) -> ConfigType:
    parent = get_parent_config(config)
    # Use the parent's mode to generate matching number traits.
    config[CONF_EXTENDED_MODE] = parent.get(CONF_EXTENDED_MODE, False)
    for key, default in ((CONF_TEMPERATURE_HIGH, 80.0), (CONF_TEMPERATURE_LOW, 75.0)):
        if key in config:
            config[key][_KEY_INITIAL_VALUE] = config[key].get(
                CONF_INITIAL_VALUE, parent.get(key, default)
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_NUMBER")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    cg.add(parent.set_configure(True))
    for key, setter, initial_setter in (
        (
            CONF_TEMPERATURE_HIGH,
            parent.set_high_limit_control,
            parent.set_temperature_high,
        ),
        (
            CONF_TEMPERATURE_LOW,
            parent.set_low_limit_control,
            parent.set_temperature_low,
        ),
    ):
        if limit_config := config.get(key):
            var = await number.new_number(
                limit_config,
                parent,
                LIMIT_TYPES[key],
                min_value=-55.0,
                max_value=150.0 if config[CONF_EXTENDED_MODE] else 127.9375,
                step=0.0625,
            )
            await cg.register_component(var, limit_config)
            cg.add(var.set_initial_value(limit_config[_KEY_INITIAL_VALUE]))
            cg.add(var.set_restore_value(limit_config[CONF_RESTORE_VALUE]))
            cg.add(initial_setter(limit_config[_KEY_INITIAL_VALUE]))
            cg.add(setter(var))
