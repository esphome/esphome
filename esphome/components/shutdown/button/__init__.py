import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_CONFIG,
    ICON_POWER,
)

shutdown_ns = cg.esphome_ns.namespace("shutdown")
ShutdownButton = shutdown_ns.class_("ShutdownButton", button.Button, cg.Component)

CONFIG_SCHEMA = button.button_schema(
    ShutdownButton, entity_category=ENTITY_CATEGORY_CONFIG, icon=ICON_POWER
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
