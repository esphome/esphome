import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv

from ..spa06_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["spa06_base"]
CODEOWNERS = ["@danielkent-net"]
DEPENDENCIES = ["i2c"]

spa06_ns = cg.esphome_ns.namespace("spa06_i2c")
SPA06I2CComponent = spa06_ns.class_(
    "SPA06I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x77)
).extend({cv.GenerateID(): cv.declare_id(SPA06I2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
