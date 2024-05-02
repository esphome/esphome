import esphome.codegen as cg
from esphome.components import i2c
from ..bmp3xx_base import to_code_base, cv, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["bmp3xx_base"]
CODEOWNERS = ["@latonita"]
DEPENDENCIES = ["i2c"]

bmp3xx_ns = cg.esphome_ns.namespace("bmp3xx_i2c")

BMP3XXI2CComponent = bmp3xx_ns.class_(
    "BMP3XXI2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x77)
).extend({cv.GenerateID(): cv.declare_id(BMP3XXI2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
