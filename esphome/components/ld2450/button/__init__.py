import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_ID,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from .. import CONF_LD2450_ID, LD2450Component, ld2450_ns

FactoryResetButton = ld2450_ns.class_("FactoryResetButton", button.Button)
RestartButton = ld2450_ns.class_("RestartButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        FactoryResetButton,
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
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_FACTORY_RESET, hub.set_factory_reset_button)
    await buttons(CONF_RESTART, hub.set_restart_button)
