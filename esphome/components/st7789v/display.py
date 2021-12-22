import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import (
    CONF_BACKLIGHT_PIN,
    CONF_BRIGHTNESS,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RESET_PIN,
)
from . import st7789v_ns

CODEOWNERS = ["@kbx81"]

DEPENDENCIES = ["spi"]

ST7789V = st7789v_ns.class_(
    "ST7789V", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)
ST7789VRef = ST7789V.operator("ref")

CONFIG_SCHEMA = (
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ST7789V),
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(spi.spi_device_schema())
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    dc = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    reset = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))

    if CONF_BACKLIGHT_PIN in config:
        bl = await cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
        cg.add(var.set_backlight_pin(bl))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))

    await display.register_display(var, config)
