import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from .. import TLC5947, tlc5947_ns

DEPENDENCIES = ["tlc5947"]

TLC5947Channel = tlc5947_ns.class_(
    "TLC5947Channel", output.FloatOutput, cg.Parented.template(TLC5947)
)

CONF_TLC5947_ID = "tlc5947_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_TLC5947_ID): cv.use_id(TLC5947),
        cv.Required(CONF_ID): cv.declare_id(TLC5947Channel),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=65535),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_TLC5947_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
