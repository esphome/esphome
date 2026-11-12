from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_INVERTED, CONF_RESET_PIN

from . import cst3240_i2c_ns

I2C_ADDRESS = 0x5A

CST3240Touchscreen = cst3240_i2c_ns.class_(
    "CST3240Touchscreen", touchscreen.Touchscreen, i2c.I2CDevice
)

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CST3240Touchscreen),
        cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pullup_pin_schema,
        cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(i2c.i2c_device_schema(I2C_ADDRESS))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if CONF_RESET_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))

    if CONF_INTERRUPT_PIN in config:
        # set pin inverted: true (if not set)
        pin_config = config[CONF_INTERRUPT_PIN].copy()
        if not pin_config.get(CONF_INVERTED, False):
            pin_config[CONF_INVERTED] = True
        pin = await cg.gpio_pin_expression(pin_config)
        cg.add(var.set_interrupt_pin(pin))
