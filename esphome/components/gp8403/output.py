import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import i2c, output
from esphome.const import CONF_ID, CONF_CHANNEL

# CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["i2c"]

gp8403_ns = cg.esphome_ns.namespace("gp8403")
GP8403Output = gp8403_ns.class_(
    "GP8403Output", cg.Component, i2c.I2CDevice, output.FloatOutput
)

CONFIG_SCHEMA = (
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(GP8403Output),
            cv.Required(CONF_CHANNEL): cv.one_of(0, 1, 2),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x58))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await output.register_output(var, config)

    cg.add(var.set_channel(config[CONF_CHANNEL]))
