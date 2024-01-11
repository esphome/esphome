import esphome.codegen as cg
from esphome.components import i2c
from ..bme280_base.sensor import to_code as to_code_base, cv, CONFIG_SCHEMA_BASE

DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["bme280_base"]

bme280_ns = cg.esphome_ns.namespace("bme280_i2c")
BME280I2CComponent = bme280_ns.class_(
    "BME280I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    i2c.i2c_device_schema(default_address=0x77)
).extend({cv.GenerateID(): cv.declare_id(BME280I2CComponent)})


async def to_code(config):
    await to_code_base(config, func=i2c.register_i2c_device)
