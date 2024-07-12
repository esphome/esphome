import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART,
)

restart_ns = cg.esphome_ns.namespace("restart")
RestartButton = restart_ns.class_("RestartButton", button.Button, cg.Component)

CONFIG_SCHEMA = button.button_schema(
    RestartButton,
    icon=ICON_RESTART,
    device_class=DEVICE_CLASS_RESTART,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
