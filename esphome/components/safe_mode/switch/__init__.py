import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import (
    CONF_SAFE_MODE,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import safe_mode_ns, SafeModeComponent

DEPENDENCIES = ["safe_mode"]

SafeModeSwitch = safe_mode_ns.class_("SafeModeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(
        SafeModeSwitch,
        block_inverted=True,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    )
    .extend({cv.GenerateID(CONF_SAFE_MODE): cv.use_id(SafeModeComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    safe_mode_component = await cg.get_variable(config[CONF_SAFE_MODE])
    cg.add(var.set_safe_mode(safe_mode_component))
