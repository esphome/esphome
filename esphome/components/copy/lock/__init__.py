import esphome.codegen as cg
from esphome.components import lock
import esphome.config_validation as cv
from esphome.const import CONF_ENTITY_CATEGORY, CONF_ICON, CONF_SOURCE_ID
from esphome.core.entity_helpers import inherit_property_from

from .. import copy_ns

CopyLock = copy_ns.class_("CopyLock", lock.Lock, cg.Component)


CONFIG_SCHEMA = (
    lock.lock_schema(CopyLock)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(lock.Lock),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
)


async def to_code(config):
    var = await lock.new_lock(config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
