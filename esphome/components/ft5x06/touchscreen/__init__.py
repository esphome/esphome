import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import i2c, touchscreen
from esphome.const import CONF_ID
from .. import ft5x06_ns

FT5x06ButtonListener = ft5x06_ns.class_("FT5x06ButtonListener")
FT5x06Touchscreen = ft5x06_ns.class_(
    "FT5x06Touchscreen",
    touchscreen.Touchscreen,
    cg.Component,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(FT5x06Touchscreen),
    }
).extend(i2c.i2c_device_schema(0x48))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)
