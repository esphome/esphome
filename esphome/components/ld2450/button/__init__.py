import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_FACTORY_RESET, CONF_ID, CONF_RESTART
from esphome.types import ConfigType

from .. import CONF_LD2450_ID, LD2450Component

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        button.ApplyButton
    ),
    cv.Optional(CONF_RESTART): button.restart_button_schema(button.ApplyButton),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_FACTORY_RESET, hub.set_factory_reset_button, apply="factory_reset()"
    )
    await buttons(
        CONF_RESTART, hub.set_restart_button, apply="restart_and_read_all_info()"
    )
