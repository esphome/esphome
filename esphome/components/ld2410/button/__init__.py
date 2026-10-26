import esphome.codegen as cg
from esphome.components import button
from esphome.components.const import CONF_QUERY_PARAMS
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_ID,
    CONF_RESTART,
    ENTITY_CATEGORY_CONFIG,
)
from esphome.types import ConfigType

from .. import CONF_LD2410_ID, LD2410Component

CONF_BACKGROUND_CORRECTION = "background_correction"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        button.ApplyButton
    ),
    cv.Optional(CONF_RESTART): button.restart_button_schema(button.ApplyButton),
    cv.Optional(CONF_QUERY_PARAMS): button.query_button_schema(button.ApplyButton),
    cv.Optional(CONF_BACKGROUND_CORRECTION): button.button_schema(
        button.ApplyButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:broom",
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_FACTORY_RESET, hub.set_factory_reset_button, apply="factory_reset()"
    )
    await buttons(
        CONF_RESTART, hub.set_restart_button, apply="restart_and_read_all_info()"
    )
    await buttons(CONF_QUERY_PARAMS, hub.set_query_button, apply="read_all_info()")
    await buttons(
        CONF_BACKGROUND_CORRECTION,
        hub.set_background_correction_button,
        apply="start_background_correction()",
    )
