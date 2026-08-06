import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.lis2dh12_base import (
    CONFIG_SCHEMA_BASE,
    LIS2DH12Component,
    to_code_base,
)
import esphome.config_validation as cv

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["lis2dh12_base"]
DEPENDENCIES = ["i2c"]

MULTI_CONF = True

lis2dh12_i2c_ns = cg.esphome_ns.namespace("lis2dh12_i2c")
LIS2DH12I2CComponent = lis2dh12_i2c_ns.class_(
    "LIS2DH12I2CComponent", LIS2DH12Component, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    {
        cv.GenerateID(): cv.declare_id(LIS2DH12I2CComponent),
    }
).extend(i2c.i2c_device_schema(0x19))


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
