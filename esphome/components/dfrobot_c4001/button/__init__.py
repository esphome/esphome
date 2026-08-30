import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    DEVICE_CLASS_UPDATE,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)

from .. import CONF_DFROBOT_C4001_ID, HUB_CHILD_SCHEMA, dfrobot_c4001_ns

CONF_CONFIG_SAVE = "config_save"

ConfigSaveButton = dfrobot_c4001_ns.class_("ConfigSaveButton", button.Button)
FactoryResetButton = dfrobot_c4001_ns.class_("FactoryResetButton", button.Button)
RestartButton = dfrobot_c4001_ns.class_("RestartButton", button.Button)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.Optional(CONF_CONFIG_SAVE): button.button_schema(
                ConfigSaveButton,
                device_class=DEVICE_CLASS_UPDATE,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_FACTORY_RESET): button.button_schema(
                FactoryResetButton,
                device_class=DEVICE_CLASS_RESTART,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_RESTART_ALERT,
            ),
            cv.Optional(CONF_RESTART): button.button_schema(
                RestartButton,
                device_class=DEVICE_CLASS_RESTART,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_RESTART,
            ),
        }
    )
    .extend(HUB_CHILD_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    sens0609_hub = await cg.get_variable(config[CONF_DFROBOT_C4001_ID])
    if config_save := config.get(CONF_CONFIG_SAVE):
        b = await button.new_button(config_save)
        await cg.register_parented(b, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_config_save_button(b))
    if factory_reset := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset)
        await cg.register_parented(b, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_factory_reset_button(b))
    if restart := config.get(CONF_RESTART):
        b = await button.new_button(restart)
        await cg.register_parented(b, config[CONF_DFROBOT_C4001_ID])
        cg.add(sens0609_hub.set_restart_button(b))
