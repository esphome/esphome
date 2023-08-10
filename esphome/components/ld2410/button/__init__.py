import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_CONFIG,
    DEVICE_CLASS_UPDATE,
)
from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

LD2410Button = ld2410_ns.class_("LD2410Button", button.Button)

CONF_FACTORY_RESET = "factory_reset"
CONF_RESTART = "restart"
CONF_QUERY_PARAMS = "query_params"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        LD2410Button,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:restart-off",
    ),
    cv.Optional(CONF_RESTART): button.button_schema(
        LD2410Button,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:restart",
    ),
    cv.Optional(CONF_QUERY_PARAMS): button.button_schema(
        LD2410Button,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        device_class=DEVICE_CLASS_UPDATE,
        icon="mdi:database-search",
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_FACTORY_RESET in config:
        b = await button.new_button(config[CONF_FACTORY_RESET])
        cg.add(ld2410_component.set_reset_button(b))
    if CONF_RESTART in config:
        b = await button.new_button(config[CONF_RESTART])
        cg.add(ld2410_component.set_restart_button(b))
    if CONF_QUERY_PARAMS in config:
        b = await button.new_button(config[CONF_QUERY_PARAMS])
        cg.add(ld2410_component.set_query_button(b))
