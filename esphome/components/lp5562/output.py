import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import LP5562Output, lp5562_ns

DEPENDENCIES = ["lp5562"]

LP5562Channel = lp5562_ns.class_("LP5562Channel", output.FloatOutput)
CONF_LP5562_ID = "lp5562_id"

# Indices match the LP5562_REG_PWM lookup table in lp5562_output.cpp
LP5562_CHANNELS = {
    "blue": 0,
    "green": 1,
    "red": 2,
    "white": 3,
}

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(LP5562Channel),
        cv.GenerateID(CONF_LP5562_ID): cv.use_id(LP5562Output),
        cv.Required(CONF_CHANNEL): cv.enum(LP5562_CHANNELS, lower=True),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_LP5562_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(parent.register_channel(var))
    await output.register_output(var, config)
