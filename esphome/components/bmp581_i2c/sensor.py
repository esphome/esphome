import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

from ..bmp581_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["bmp581_base"]
CODEOWNERS = ["@kahrendt", "@danielkent-net"]
DEPENDENCIES = ["i2c"]

bmp581_ns = cg.esphome_ns.namespace("bmp581_i2c")
BMP581I2CComponent = bmp581_ns.class_(
    "BMP581I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x46)
).extend({cv.GenerateID(): cv.declare_id(BMP581I2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
