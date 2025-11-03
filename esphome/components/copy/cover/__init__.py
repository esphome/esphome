import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_SOURCE_ID,
)
from esphome.core.entity_helpers import inherit_property_from

from .. import copy_ns

CopyCover = copy_ns.class_("CopyCover", cover.Cover, cg.Component)


CONFIG_SCHEMA = (
    cover.cover_schema(CopyCover)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(cover.Cover),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
    inherit_property_from(CONF_DEVICE_CLASS, CONF_SOURCE_ID),
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
