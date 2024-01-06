import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.components.ota import OTAComponent
from esphome.const import (
    CONF_ID,
    CONF_OTA,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)

DEPENDENCIES = ["ota"]

safe_mode_ns = cg.esphome_ns.namespace("safe_mode")
SafeModeButton = safe_mode_ns.class_("SafeModeButton", button.Button, cg.Component)

CONFIG_SCHEMA = (
    button.button_schema(
        SafeModeButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    )
    .extend({cv.GenerateID(CONF_OTA): cv.use_id(OTAComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await button.register_button(var, config)

    ota = await cg.get_variable(config[CONF_OTA])
    cg.add(var.set_ota(ota))
