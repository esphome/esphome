import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import i2c, output
from esphome.const import CONF_ID

from .. import gp8211_ns, GP8211, CONF_GP8211_ID

DEPENDENCIES = ["gp8211"]

GP8211Output = gp8211_ns.class_(
    "GP8211Output", cg.Component, i2c.I2CDevice, output.FloatOutput
)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GP8211Output),
        cv.GenerateID(CONF_GP8211_ID): cv.use_id(GP8211),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    await cg.register_parented(var, config[CONF_GP8211_ID])
