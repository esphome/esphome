import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

from ..lis3dh import CONFIG_SCHEMA_BASE, LIS3DHComponent, setup_lis3dh_base

AUTO_LOAD = ["lis3dh"]
CODEOWNERS = ["@jthoward64"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

lis3dh_i2c_ns = cg.esphome_ns.namespace("lis3dh_i2c")
LIS3DHI2C = lis3dh_i2c_ns.class_("LIS3DHI2C", LIS3DHComponent, i2c.I2CDevice)

CONFIG_SCHEMA = (
    CONFIG_SCHEMA_BASE.extend({cv.GenerateID(): cv.declare_id(LIS3DHI2C)})
).extend(i2c.i2c_device_schema(0x18))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await setup_lis3dh_base(var, config)
