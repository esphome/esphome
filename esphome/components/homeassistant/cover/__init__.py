import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover
from esphome.const import CONF_ID
from esphome import automation
from esphome.automation import Condition, maybe_simple_id

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

COVER_CONDITION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(HomeassistantCover),
    }
)

# Condition
CoverCondition = homeassistant_ns.class_("CoverCondition", Condition)


@automation.register_condition(
    "cover.is_unavailable", CoverCondition, COVER_CONDITION_SCHEMA
)
async def cover_is_unavailable_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, True)


@automation.register_condition(
    "cover.is_unknown", CoverCondition, COVER_CONDITION_SCHEMA
)
async def cover_is_unknown_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren, False)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    setup_home_assistant_entity(var, config)
