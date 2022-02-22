import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_SOURCE_ID,
)
from esphome.core.entity_helpers import inherit_property_from

from .. import copy_ns

CopyButton = copy_ns.class_("CopyButton", button.Button, cg.Component)


CONFIG_SCHEMA = (
    button.button_schema()
    .extend(
        {
            cv.GenerateID(): cv.declare_id(CopyButton),
            cv.Required(CONF_SOURCE_ID): cv.use_id(button.Button),
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
    var = cg.new_Pvariable(config[CONF_ID])
    await button.register_button(var, config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
