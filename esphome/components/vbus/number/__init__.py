"""Confirmed numeric setting controls for DeltaSol CS Plus 1.11."""

from functools import partial

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_UNIT_OF_MEASUREMENT, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import (
    CONF_CONTROLLER_ID,
    CONF_PARAMETER,
    ResolDeltaSolCSPlus,
    register_setting_entity,
    setting_configs,
    setting_entity_defaults,
    setting_group_defaults,
    setting_platform_schema,
    validate_setting_controller,
    vbus_ns,
)
from ..cs_plus_numbers import NUMBER_PARAMETERS

DEPENDENCIES = ["vbus"]
VBusSettingNumber = vbus_ns.class_("VBusSettingNumber", number.Number)


def setting_defaults(config: ConfigType) -> ConfigType:
    config = setting_entity_defaults(config)
    if isinstance(config, dict) and (
        spec := NUMBER_PARAMETERS.get(str(config.get(CONF_PARAMETER)).lower())
    ):
        config = config.copy()
        config.setdefault(CONF_UNIT_OF_MEASUREMENT, spec.unit)
    return config


ENTITY_SCHEMA = number.number_schema(
    VBusSettingNumber, entity_category=ENTITY_CATEGORY_CONFIG
)
SINGLE_SCHEMA = cv.All(
    setting_defaults,
    ENTITY_SCHEMA.extend(
        {
            cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            cv.Required(CONF_PARAMETER): cv.one_of(*NUMBER_PARAMETERS, lower=True),
        }
    ),
)
GROUP_SCHEMA = cv.All(
    partial(
        setting_group_defaults, parameters=NUMBER_PARAMETERS, defaults=setting_defaults
    ),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(cg.EntityBase),
            cv.GenerateID(CONF_CONTROLLER_ID): cv.use_id(ResolDeltaSolCSPlus),
            **{cv.Optional(key): ENTITY_SCHEMA for key in NUMBER_PARAMETERS},
        }
    ),
    cv.has_at_least_one_key(*NUMBER_PARAMETERS),
)
CONFIG_SCHEMA = setting_platform_schema(
    NUMBER_PARAMETERS, ENTITY_SCHEMA, SINGLE_SCHEMA, GROUP_SCHEMA
)
FINAL_VALIDATE_SCHEMA = validate_setting_controller


async def to_code(config: ConfigType) -> None:
    controller = await cg.get_variable(config[CONF_CONTROLLER_ID])
    for entity in setting_configs(config, NUMBER_PARAMETERS):
        spec = NUMBER_PARAMETERS[entity[CONF_PARAMETER]]
        var = await number.new_number(
            entity,
            controller,
            spec.index,
            min_value=spec.minimum * spec.scale,
            max_value=spec.maximum * spec.scale,
            step=spec.step * spec.scale,
        )
        register_setting_entity(controller, var)
