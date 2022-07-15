import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import input_text
from esphome.const import (
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_MODE,
    CONF_SOURCE_ID,
)
from esphome.core.entity_helpers import inherit_property_from

from .. import copy_ns

CopyInputText = copy_ns.class_("CopyInputText", input_text.InputText, cg.Component)


CONFIG_SCHEMA = input_text.INPUT_TEXT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CopyInputText),
        cv.Required(CONF_SOURCE_ID): cv.use_id(input_text.InputText),
    }
).extend(cv.COMPONENT_SCHEMA)

FINAL_VALIDATE_SCHEMA = cv.All(
    inherit_property_from(CONF_ICON, CONF_SOURCE_ID),
    inherit_property_from(CONF_ENTITY_CATEGORY, CONF_SOURCE_ID),
    inherit_property_from(CONF_MODE, CONF_SOURCE_ID),
)


async def to_code(config):
    var = await input_text.new_input_text(config)
    await cg.register_component(var, config)

    source = await cg.get_variable(config[CONF_SOURCE_ID])
    cg.add(var.set_source(source))
