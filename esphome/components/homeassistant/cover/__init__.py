import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID
from esphome.automation import ENTITY_STATE_TRIGGER_SCHEMA, setup_entity_state_trigger

from .. import (
    HOME_ASSISTANT_IMPORT_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

DEPENDENCIES = ["api"]

HomeassistantCover = homeassistant_ns.class_(
    "HomeassistantCover", cover.Cover, cg.Component, cg.EntityBase_State
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend({cv.GenerateID(): cv.declare_id(HomeassistantCover)})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(HOME_ASSISTANT_IMPORT_SCHEMA)
    .extend(ENTITY_STATE_TRIGGER_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    await setup_entity_state_trigger(var, config)
    setup_home_assistant_entity(var, config)
