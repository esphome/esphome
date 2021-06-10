import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import TLC59208FOutput, tlc59208f_ns

DEPENDENCIES = ["tlc59208f"]

TLC59208FChannel = tlc59208f_ns.class_("TLC59208FChannel", output.FloatOutput)
CONF_TLC59208F_ID = "tlc59208f_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(TLC59208FChannel),
        cv.GenerateID(CONF_TLC59208F_ID): cv.use_id(TLC59208FOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_TLC59208F_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(paren.register_channel(var))
    await output.register_output(var, config)
