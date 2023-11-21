import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import i2c, touchscreen
from esphome.const import CONF_INTERRUPT_PIN, CONF_ID, CONF_ROTATION
from .. import gt911_ns


GT911ButtonListener = gt911_ns.class_("GT911ButtonListener")
GT911Touchscreen = gt911_ns.class_(
    "GT911Touchscreen",
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
            cv.GenerateID(): cv.declare_id(GT911Touchscreen),
            cv.Optional(CONF_ROTATION): cv.enum(ROTATIONS),
            cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x5D))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await touchscreen.register_touchscreen(var, config)

    interrupt_pin = await cg.gpio_pin_expression(config[CONF_INTERRUPT_PIN])
    cg.add(var.set_interrupt_pin(interrupt_pin))
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(ROTATIONS[config[CONF_ROTATION]]))
