"""Confirmed numeric setting controls for DeltaSol CS Plus 1.11."""

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import (
    CONF_CONTROLLER_ID,
    CONF_PARAMETER,
    ResolDeltaSolCSPlus,
    register_setting_entity,
    require_controller_writes,
    vbus_ns,
)
from ..cs_plus_numbers import NUMBER_PARAMETERS

DEPENDENCIES = ["vbus"]
VBusSettingNumber = vbus_ns.class_("VBusSettingNumber", number.Number)


def validate_binding(config: ConfigType) -> ConfigType:
    require_controller_writes(config[CONF_CONTROLLER_ID])
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(
        VBusSettingNumber, entity_category=ENTITY_CATEGORY_CONFIG
    ).extend(
        {
            cv.Required(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            cv.Required(CONF_PARAMETER): cv.one_of(*NUMBER_PARAMETERS, lower=True),
        }
    ),
    validate_binding,
)


async def to_code(config: ConfigType) -> None:
    spec = NUMBER_PARAMETERS[config[CONF_PARAMETER]]
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    var = await number.new_number(
        config,
        controller,
        spec.index,
        min_value=spec.minimum * spec.scale,
        max_value=spec.maximum * spec.scale,
        step=spec.step * spec.scale,
    )
    register_setting_entity(controller, var)
