import esphome.codegen as cg
from esphome.components import button
from esphome.components.const import CONF_QUERY_PARAMS
import esphome.config_validation as cv
from esphome.const import CONF_FACTORY_RESET, CONF_ID, CONF_RESTART
from esphome.types import ConfigType

from .. import CONF_LD2410_ID, LD2410Component, ld2410_ns

FactoryResetButton = ld2410_ns.class_("FactoryResetButton", button.Button)
QueryButton = ld2410_ns.class_("QueryButton", button.Button)
RestartButton = ld2410_ns.class_("RestartButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        FactoryResetButton
    ),
    cv.Optional(CONF_RESTART): button.restart_button_schema(RestartButton),
    cv.Optional(CONF_QUERY_PARAMS): button.query_button_schema(QueryButton),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_FACTORY_RESET, hub.set_factory_reset_button)
    await buttons(CONF_RESTART, hub.set_restart_button)
    await buttons(CONF_QUERY_PARAMS, hub.set_query_button)
