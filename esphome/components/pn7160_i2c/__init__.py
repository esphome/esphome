import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

from .. import pn7160

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["pn7160"]

pn7160_i2c_ns = cg.esphome_ns.namespace("pn7160_i2c")
PN7160I2C = pn7160_i2c_ns.class_("PN7160I2C", pn7160.PN7160, i2c.I2CDevice)

CONFIG_SCHEMA = (
    pn7160.PN7160_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(PN7160I2C),
        }
    )
    .extend(i2c.i2c_device_schema(0x28))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await pn7160.setup_pn7160(var, config)

