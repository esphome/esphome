"""Adjustable TMP102 thermostat thresholds."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
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
    TMP102_CONVERSION_FACTOR,
    TMP102_MIN_TEMPERATURE,
    TMP102Component,
    TMP102LimitType,
    add_number_parent,
    max_temperature,
    number_parent_extended,
    tmp102_ns,
    validate_unique_child,
)

TMP102LimitNumber = tmp102_ns.class_(
    "TMP102LimitNumber", number.Number, cg.Parented.template(TMP102Component)
)

_LIMIT_SCHEMA = number.number_schema(
    TMP102LimitNumber,
    unit_of_measurement=UNIT_CELSIUS,
    device_class=DEVICE_CLASS_TEMPERATURE,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend({cv.Optional(CONF_RESTORE_VALUE, default=True): cv.boolean})

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


def _final_validate(config: ConfigType) -> None:
    validate_unique_child(config, "number")
    full = fv.full_config.get()
    parent = full.get_config_for_path(full.get_path_for_id(config[CONF_TMP102_ID])[:-1])
    add_number_parent(config[CONF_TMP102_ID], parent.get(CONF_EXTENDED_MODE, False))


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_NUMBER")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    limits = {
        "min_value": TMP102_MIN_TEMPERATURE,
        "max_value": max_temperature(number_parent_extended(config[CONF_TMP102_ID])),
        "step": TMP102_CONVERSION_FACTOR,
    }
    numbers = number.sub_numbers(config, parent=parent)
    if var := await numbers(
        CONF_TEMPERATURE_HIGH,
        parent.set_limit_control,
        TMP102LimitType.TMP102_LIMIT_HIGH,
        **limits,
    ):
        cg.add(var.set_restore_value(config[CONF_TEMPERATURE_HIGH][CONF_RESTORE_VALUE]))
    if var := await numbers(
        CONF_TEMPERATURE_LOW,
        parent.set_limit_control,
        TMP102LimitType.TMP102_LIMIT_LOW,
        **limits,
    ):
        cg.add(var.set_restore_value(config[CONF_TEMPERATURE_LOW][CONF_RESTORE_VALUE]))
