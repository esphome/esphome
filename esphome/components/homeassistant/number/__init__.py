import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

CODEOWNERS = ["@landonr"]
DEPENDENCIES = ["api"]

HomeassistantNumber = homeassistant_ns.class_(
    "HomeassistantNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = (
    number.number_schema(HomeassistantNumber)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await number.new_number(
        config,
        min_value=0,
        max_value=0,
        step=0,
    )
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
