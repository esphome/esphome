import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantCover = homeassistant_ns.class_(
    "HomeassistantCover", cover.Cover, cg.Component
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend({cv.GenerateID(): cv.declare_id(HomeassistantCover)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(HOME_ASSISTANT_IMPORT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    setup_home_assistant_entity(var, config)
