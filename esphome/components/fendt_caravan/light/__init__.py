from functools import lru_cache

import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_ID,
    CONF_OUTPUT_ID,
    CONF_TYPE,
)
from esphome.core import CORE

from .. import CONF_PARENT_ID, FendtCaravanHubBase, fendt_caravan_ns

CONF_LIGHT_TYPE = "light_type"
CONF_VARIABLE_NAME = "variable_name"


LightingDeviceHub = fendt_caravan_ns.class_("LightingDeviceHub", FendtCaravanHubBase)

FendtBinaryLightOutput = fendt_caravan_ns.class_(
    "FendtBinaryLightOutput",
    light.LightOutput,
    cg.Parented.template(LightingDeviceHub),
)

FendtFloatLightOutput = fendt_caravan_ns.class_(
    "FendtFloatLightOutput",
    light.LightOutput,
    cg.Parented.template(LightingDeviceHub),
)


def _binary_output_schema() -> cv.Schema:
    return light.BINARY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FendtBinaryLightOutput),
            cv.Required(CONF_PARENT_ID): cv.use_id(LightingDeviceHub),
            cv.Required(CONF_LIGHT_TYPE): cv.one_of("switch", "dimmable", lower=True),
            cv.Required(CONF_VARIABLE_NAME): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA)


def _float_output_schema(key_name_=cv.UNDEFINED) -> cv.Schema:
    return light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FendtFloatLightOutput),
            cv.Required(CONF_PARENT_ID): cv.use_id(LightingDeviceHub),
            cv.Required(CONF_LIGHT_TYPE): cv.one_of("switch", "dimmable", lower=True),
            cv.Required(CONF_VARIABLE_NAME): cv.string,
            cv.Optional(
                CONF_DEFAULT_TRANSITION_LENGTH, default="0s"
            ): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA)


@lru_cache(maxsize=1)
def build_static_schema_dict():
    schema_dict = {}
    raw_light_config = CORE.raw_config.get("light", [])
    if isinstance(raw_light_config, list):
        for light_config in raw_light_config:
            if light_config.get("platform") == "fendt_caravan":
                conf_id = light_config.get(CONF_ID)
                light_type = light_config.get(CONF_LIGHT_TYPE)
                if light_type == "switch":
                    schema_dict[conf_id] = _binary_output_schema()
                if light_type == "dimmable":
                    schema_dict[conf_id] = _float_output_schema()
    return schema_dict


def dynamic_caravan_lights_validator(value):
    valid_schemas = build_static_schema_dict()
    value[CONF_TYPE] = value[CONF_ID]
    return cv.typed_schema(valid_schemas)(value)


CONFIG_SCHEMA = dynamic_caravan_lights_validator


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_parented(var, parent)
    cg.add(var.set_variable_name(config[CONF_VARIABLE_NAME]))
    if config.get(CONF_LIGHT_TYPE) == "switch":
        cg.add(parent.add_binary_lightoutput(config[CONF_VARIABLE_NAME], var))
    if config.get(CONF_LIGHT_TYPE) == "dimmable":
        cg.add(parent.add_float_lightoutput(config[CONF_VARIABLE_NAME], var))
