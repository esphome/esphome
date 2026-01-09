import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@gabest11"]
MULTI_CONF = True

i2c_device_ns = cg.esphome_ns.namespace("i2c_device")

I2CDeviceComponent = i2c_device_ns.class_(
    "I2CDeviceComponent", cg.Component, i2c.I2CDevice
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(I2CDeviceComponent),
    }
).extend(i2c.i2c_device_schema(None))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
