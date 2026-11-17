import esphome.codegen as cg
from esphome.components import button
from esphome.components.const import CONF_QUERY_PARAMS
import esphome.config_validation as cv
from esphome.const import CONF_FACTORY_RESET, CONF_ID, CONF_RESTART
from esphome.types import ConfigType

from . import CONF_DFROBOT_SEN0609_ID, DFRobotSEN0609Component

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_FACTORY_RESET): button.factory_reset_button_schema(
        button.ApplyButton
    ),
    cv.Optional(CONF_QUERY_PARAMS): button.query_button_schema(button.ApplyButton),
    cv.Optional(CONF_RESTART): button.restart_button_schema(button.ApplyButton),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_FACTORY_RESET, hub.set_factory_reset_button, apply="factory_reset()"
    )
    await buttons(CONF_QUERY_PARAMS, hub.set_query_button, apply="query_params()")
    await buttons(CONF_RESTART, hub.set_restart_button, apply="restart()")
