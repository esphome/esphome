import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from esphome.types import ConfigType

from .. import CONF_MR24HPC1_ID, MR24HPC1Component, mr24hpc1_ns

RestartButton = mr24hpc1_ns.class_("RestartButton", button.Button)
CustomSetEndButton = mr24hpc1_ns.class_("CustomSetEndButton", button.Button)

CONF_CUSTOM_SET_END = "custom_set_end"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_MR24HPC1_ID): cv.use_id(MR24HPC1Component),
    cv.Optional(CONF_RESTART): button.button_schema(
        RestartButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    ),
    cv.Optional(CONF_CUSTOM_SET_END): button.button_schema(
        CustomSetEndButton,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:cog",
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MR24HPC1_ID])
    buttons = button.sub_buttons(config, parent=hub)
    await buttons(CONF_RESTART, hub.set_restart_button)
    await buttons(CONF_CUSTOM_SET_END, hub.set_custom_set_end_button)
