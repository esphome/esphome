import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, touchscreen
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERRUPT_PIN, CONF_RESET_PIN

LOGGER = logging.getLogger(__name__)

DEPENDENCIES = ["i2c"]

axs5106_ns = cg.esphome_ns.namespace("axs5106")

AXS5106Component = axs5106_ns.class_(
    "AXS5106Touchscreen", touchscreen.Touchscreen, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    touchscreen.touchscreen_schema("100ms")
    .extend(
        {
            cv.GenerateID(): cv.declare_id(AXS5106Component),
            cv.Optional(CONF_INTERRUPT_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(i2c.i2c_device_schema(0x63))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await touchscreen.register_touchscreen(var, config)
    await i2c.register_i2c_device(var, config)

    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        cg.add(var.set_interrupt_pin(await cg.gpio_pin_expression(interrupt_pin)))
    if reset_pin := config.get(CONF_RESET_PIN):
        cg.add(var.set_reset_pin(await cg.gpio_pin_expression(reset_pin)))
