import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)

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


async def to_code(config):
    mr24hpc1_component = await cg.get_variable(config[CONF_MR24HPC1_ID])
    if restart_config := config.get(CONF_RESTART):
        b = await button.new_button(restart_config)
        await cg.register_parented(b, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_restart_button(b))
    if custom_set_end_config := config.get(CONF_CUSTOM_SET_END):
        b = await button.new_button(custom_set_end_config)
        await cg.register_parented(b, config[CONF_MR24HPC1_ID])
        cg.add(mr24hpc1_component.set_custom_set_end_button(b))
