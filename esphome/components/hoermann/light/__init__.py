import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID

from .. import CONF_HOERMANN_ID, Hoermann, hoermann_ns

DEPENDENCIES = ["hoermann"]

HoermannLight = hoermann_ns.class_("HoermannLight", light.LightOutput, cg.Component)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HoermannLight),
        cv.GenerateID(CONF_HOERMANN_ID): cv.use_id(Hoermann),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)
    parent = await cg.get_variable(config[CONF_HOERMANN_ID])
    cg.add(var.set_parent(parent))
