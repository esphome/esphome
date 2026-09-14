import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import (
    CONF_FACTORY_RESET,
    CONF_RESTART,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_RESTART,
    ICON_RESTART_ALERT,
)

from .. import CONF_TFLUNA_ID, TFLunaComponent, tfluna_ns

ResetButton = tfluna_ns.class_("ResetButton", button.Button)
RestartButton = tfluna_ns.class_("RestartButton", button.Button)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_TFLUNA_ID): cv.use_id(TFLunaComponent),
    cv.Optional(CONF_FACTORY_RESET): button.button_schema(
        ResetButton,
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


async def to_code(config):
    tfluna_component = await cg.get_variable(config[CONF_TFLUNA_ID])
    if factory_reset_config := config.get(CONF_FACTORY_RESET):
        b = await button.new_button(factory_reset_config)
        await cg.register_parented(b, config[CONF_TFLUNA_ID])
        cg.add(tfluna_component.set_reset_button(b))
    if restart_config := config.get(CONF_RESTART):
        b = await button.new_button(restart_config)
        await cg.register_parented(b, config[CONF_TFLUNA_ID])
        cg.add(tfluna_component.set_restart_button(b))
