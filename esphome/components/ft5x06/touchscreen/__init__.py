import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import i2c, touchscreen
from esphome.const import CONF_INTERRUPT_PIN, CONF_ID, CONF_ROTATION
from .. import ft5x06_ns

FT5x06ButtonListener = ft5x06_ns.class_("FT5x06ButtonListener")
FT5x06Touchscreen = ft5x06_ns.class_(
    "FT5x06Touchscreen",
    touchscreen.Touchscreen,
    cg.Component,
    i2c.I2CDevice,
)

ROTATIONS = {
    0: touchscreen.TouchRotation.ROTATE_0_DEGREES,
    90: touchscreen.TouchRotation.ROTATE_90_DEGREES,
    180: touchscreen.TouchRotation.ROTATE_180_DEGREES,
    270: touchscreen.TouchRotation.ROTATE_270_DEGREES,
}
CONFIG_SCHEMA = (
    touchscreen.TOUCHSCREEN_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(FT5x06Touchscreen),
            cv.Optional(CONF_ROTATION): cv.enum(ROTATIONS),
            cv.Required(CONF_INTERRUPT_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x48))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))
    if rotation := config.get(CONF_ROTATION):
        cg.add(var.set_rotation(ROTATIONS[rotation]))
