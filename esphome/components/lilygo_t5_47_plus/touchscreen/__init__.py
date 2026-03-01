import esphome.codegen as cg
from esphome.components import esp32, i2c, touchscreen
from esphome.components.esp32.const import VARIANT_ESP32S3
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import lilygo_t5_47_plus_ns

CODEOWNERS = ["@hbast"]
DEPENDENCIES = ["i2c", "esp32"]

LilygoT547PlusTouchscreen = lilygo_t5_47_plus_ns.class_(
    "LilygoT547PlusTouchscreen",
    touchscreen.Touchscreen,
    i2c.I2CDevice,
)

CONFIG_SCHEMA = cv.All(
    touchscreen.TOUCHSCREEN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(LilygoT547PlusTouchscreen),
        }
    ).extend(i2c.i2c_device_schema(0x5D)),
    cv.only_with_arduino,
    esp32.only_on_variant(supported=[VARIANT_ESP32S3]),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)
