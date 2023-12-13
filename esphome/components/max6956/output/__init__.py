import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_PIN, CONF_ID
from .. import MAX6956, max6956_ns, CONF_MAX6956

DEPENDENCIES = ["max6956"]

MAX6956LedChannel = max6956_ns.class_(
    "MAX6956LedChannel", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(MAX6956LedChannel),
        cv.GenerateID(CONF_MAX6956): cv.use_id(MAX6956),
        cv.Required(CONF_PIN): cv.int_range(min=4, max=31),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MAX6956])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_parent(parent))
