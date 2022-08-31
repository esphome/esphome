import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.components.ota import OTAComponent
from esphome.const import (
    CONF_OTA,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import safe_mode_ns

DEPENDENCIES = ["ota"]

SafeModeSwitch = safe_mode_ns.class_("SafeModeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(
        SafeModeSwitch,
        icon=ICON_RESTART_ALERT,
        entity_category=ENTITY_CATEGORY_CONFIG,
        block_inverted=True,
    )
    .extend({cv.GenerateID(CONF_OTA): cv.use_id(OTAComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    ota = await cg.get_variable(config[CONF_OTA])
    cg.add(var.set_ota(ota))
