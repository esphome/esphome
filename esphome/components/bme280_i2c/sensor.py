import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from ..bme280_base import to_code_base, CONFIG_SCHEMA_BASE

AUTO_LOAD = ["bme280_base"]
DEPENDENCIES = ["i2c"]

bme280_ns = cg.esphome_ns.namespace("bme280_i2c")
BME280I2CComponent = bme280_ns.class_(
    "BME280I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x77)
).extend({cv.GenerateID(): cv.declare_id(BME280I2CComponent)})


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
