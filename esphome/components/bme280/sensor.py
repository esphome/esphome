import esphome.codegen as cg
from esphome.components import i2c
from ..bme280_base.sensor import (
    to_code as to_code_base,
    bme280_ns,
    CONFIG_SCHEMA_BASE,
)

DEPENDENCIES = ["i2c"]

AUTO_LOAD = ["bme280_base"]

BME280SPIComponent = bme280_ns.class_(
    "BME280I2CComponent", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(i2c.i2c_device_schema(default_address=0x77))

FUNC = i2c.register_i2c_device


async def to_code(config):
    await to_code_base(config)
