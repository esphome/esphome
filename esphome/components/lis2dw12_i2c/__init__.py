import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.lis2dw12_base import (
    CONFIG_SCHEMA_BASE,
    LIS2DW12Component,
    to_code_base,
)
import esphome.config_validation as cv

CODEOWNERS = ["@latonita"]
AUTO_LOAD = ["lis2dw12_base"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

lis2dw12_i2c_ns = cg.esphome_ns.namespace("lis2dw12_i2c")
LIS2DW12I2CComponent = lis2dw12_i2c_ns.class_(
    "LIS2DW12I2CComponent", LIS2DW12Component, i2c.I2CDevice
)

CONFIG_SCHEMA = CONFIG_SCHEMA_BASE.extend(
    {
        cv.GenerateID(): cv.declare_id(LIS2DW12I2CComponent),
    }
).extend(i2c.i2c_device_schema(0x19))


async def to_code(config):
    var = await to_code_base(config)
    await i2c.register_i2c_device(var, config)
