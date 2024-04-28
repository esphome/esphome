import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.components.esphome.ota import ESPHomeOTAComponent
from esphome.const import (
    CONF_ESPHOME,
    ENTITY_CATEGORY_CONFIG,
    ICON_RESTART_ALERT,
)
from .. import safe_mode_ns

DEPENDENCIES = ["ota.esphome"]

SafeModeSwitch = safe_mode_ns.class_("SafeModeSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = (
    switch.switch_schema(
        SafeModeSwitch,
        block_inverted=True,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_RESTART_ALERT,
    )
    .extend({cv.GenerateID(CONF_ESPHOME): cv.use_id(ESPHomeOTAComponent)})
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_component(var, config)

    ota = await cg.get_variable(config[CONF_ESPHOME])
    cg.add(var.set_ota(ota))
