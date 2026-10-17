"""Legacy fan platform that uses deprecated FanTraits setters."""

import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv
from esphome.types import ConfigType

legacy_fan_ns = cg.esphome_ns.namespace("legacy_fan_test")
LegacyFan = legacy_fan_ns.class_("LegacyFan", fan.Fan, cg.Component)

CONFIG_SCHEMA = fan.fan_schema(LegacyFan).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
