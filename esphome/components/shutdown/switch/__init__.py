import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_POWER,
)

shutdown_ns = cg.esphome_ns.namespace("shutdown")
ShutdownSwitch = shutdown_ns.class_("ShutdownSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.switch_schema(
    ShutdownSwitch,
    icon=ICON_POWER,
    entity_category=ENTITY_CATEGORY_CONFIG,
    block_inverted=True,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
