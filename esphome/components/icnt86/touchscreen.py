import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import i2c, touchscreen
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN


CODEOWNERS = ["@siemon-geeroms"]
DEPENDENCIES = ["i2c"]

icnt86_ns = cg.esphome_ns.namespace("icnt86")
ICNT86Touchscreen = icnt86_ns.class_(
    "ICNT86Touchscreen",
    touchscreen.Touchscreen,
    cg.Component,
    i2c.I2CDevice,
)

CONF_ICNT86_ID = "icnt86_id"
CONF_RTS_PIN = "rts_pin"

CONFIG_SCHEMA = touchscreen.TOUCHSCREEN_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ICNT86Touchscreen),
            cv.Required(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
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

    if CONF_RESET_PIN in config:
        rts_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(rts_pin))
