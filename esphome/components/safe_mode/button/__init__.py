import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_SAFE_MODE,
    DEVICE_CLASS_RESTART,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import safe_mode_ns, SafeModeComponent

DEPENDENCIES = ["safe_mode"]

SafeModeButton = safe_mode_ns.class_("SafeModeButton", button.Button, cg.Component)

CONFIG_SCHEMA = (
    button.button_schema(
        SafeModeButton,
        device_class=DEVICE_CLASS_RESTART,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    )
    .extend({cv.GenerateID(CONF_SAFE_MODE): cv.use_id(SafeModeComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)

    safe_mode_component = await cg.get_variable(config[CONF_SAFE_MODE])
    cg.add(var.set_safe_mode(safe_mode_component))
