import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART,
    ICON_RESTART_ALERT,
    ICON_DATABASE,
)
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

QueryButton = ld2410_ns.class_("QueryButton", button.Button)
ResetButton = ld2410_ns.class_("ResetButton", button.Button)
RestartButton = ld2410_ns.class_("RestartButton", button.Button)

CONF_FACTORY_RESET = "factory_reset"
CONF_RESTART = "restart"
CONF_QUERY_PARAMS = "query_params"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        ResetButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_RESTART): button.button_schema(
        RestartButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_RESTART,
    ),
    cv.Optional(CONF_QUERY_PARAMS): button.button_schema(
        QueryButton,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_DATABASE,
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_reset_button(b))
    if restart_config := config.get(CONF_RESTART):
        b = await button.new_button(restart_config)
        await cg.register_parented(b, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_restart_button(b))
    if query_params_config := config.get(CONF_QUERY_PARAMS):
        b = await button.new_button(query_params_config)
        await cg.register_parented(b, config[CONF_LD2410_ID])
        cg.add(ld2410_component.set_query_button(b))
