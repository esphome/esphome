import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID

from . import PCA9685Output, pca9685_ns

DEPENDENCIES = ["pca9685"]

PCA9685Channel = pca9685_ns.class_("PCA9685Channel", output.FloatOutput)
CONF_PCA9685_ID = "pca9685_id"

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(PCA9685Channel),
        cv.GenerateID(CONF_PCA9685_ID): cv.use_id(PCA9685Output),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
    }
)


async def to_code(config):
    paren = await cg.get_variable(config[CONF_PCA9685_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(paren.register_channel(var))
    await output.register_output(var, config)
