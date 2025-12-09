import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

from ..xensiv_dps3xx_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["xensiv_dps3xx_base"]
DEPENDENCIES = ["i2c"]

xensiv_dps3xx_ns = cg.esphome_ns.namespace("xensiv_dps3xx_i2c")
XENSIVDPS3xxI2C = xensiv_dps3xx_ns.class_(
    "XensivDPS3xxI2C", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x77)
).extend({cv.GenerateID(): cv.declare_id(XENSIVDPS3xxI2C)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
