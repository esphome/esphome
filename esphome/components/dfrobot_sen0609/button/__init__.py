import esphome.codegen as cg
from esphome.components import button
from esphome.components.const import CONF_QUERY_PARAMS
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_ID,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_DATABASE,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from .. import CONF_DFROBOT_SEN0609_ID, DFRobotSEN0609Component, dfrobot_sen0609_ns

DFRobotSEN0609Button = dfrobot_sen0609_ns.class_("DFRobotSEN0609Button", button.Button)
ButtonAction = dfrobot_sen0609_ns.enum("ButtonAction")

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        DFRobotSEN0609Button,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_QUERY_PARAMS): button.button_schema(
        DFRobotSEN0609Button,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_DATABASE,
    ),
    cv.Optional(CONF_RESTART): button.button_schema(
        DFRobotSEN0609Button,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_RESTART,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DFROBOT_SEN0609_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(
        CONF_FACTORY_RESET,
        hub.set_factory_reset_button,
        ButtonAction.BUTTON_FACTORY_RESET,
    )
    await buttons(
        CONF_QUERY_PARAMS, hub.set_query_button, ButtonAction.BUTTON_QUERY_PARAMS
    )
    await buttons(CONF_RESTART, hub.set_restart_button, ButtonAction.BUTTON_RESTART)
