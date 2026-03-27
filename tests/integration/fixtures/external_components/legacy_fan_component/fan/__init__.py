"""Legacy fan platform that uses deprecated FanTraits setters."""

import esphome.codegen as cg
from esphome.components import fan
import esphome.config_validation as cv

legacy_fan_ns = cg.esphome_ns.namespace("legacy_fan_test")
LegacyFan = legacy_fan_ns.class_("LegacyFan", fan.Fan, cg.Component)

CONFIG_SCHEMA = fan.FAN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LegacyFan),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
