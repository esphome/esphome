import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

CODEOWNERS = ["@Links2004"]
DEPENDENCIES = ["api"]

HomeassistantSwitch = homeassistant_ns.class_(
    "HomeassistantSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = (
    switch.switch_schema(HomeassistantSwitch)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    setup_home_assistant_entity(var, config)
