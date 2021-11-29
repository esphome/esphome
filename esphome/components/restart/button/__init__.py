import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ID,
    CONF_ICON,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART,
)

restart_ns = cg.esphome_ns.namespace("restart")
RestartButton = restart_ns.class_("RestartButton", button.Button, cg.Component)

CONFIG_SCHEMA = button.BUTTON_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RestartButton),
        cv.Optional(CONF_ICON, default=ICON_RESTART): button.icon,
        cv.Optional(
            CONF_ENTITY_CATEGORY, default=ENTITY_CATEGORY_CONFIG
        ): cv.entity_category,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)
