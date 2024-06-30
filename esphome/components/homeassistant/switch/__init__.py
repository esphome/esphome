import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantSwitch = homeassistant_ns.class_(
    "HomeassistantSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = (
    switch.switch_schema(HomeassistantSwitch)
    .extend({cv.GenerateID(): cv.declare_id(HomeassistantSwitch)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(HOME_ASSISTANT_IMPORT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    setup_home_assistant_entity(var, config)
