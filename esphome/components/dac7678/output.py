import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_CHANNEL, CONF_ID
from . import DAC7678Output, dac7678_ns

DEPENDENCIES = ["dac7678"]

DAC7678Channel = dac7678_ns.class_("DAC7678Channel", output.FloatOutput)
CONF_DAC7678_ID = "dac7678_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(DAC7678Channel),
        cv.GenerateID(CONF_DAC7678_ID): cv.use_id(DAC7678Output),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_DAC7678_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(paren.register_channel(var))
    await output.register_output(var, config)
    return var
