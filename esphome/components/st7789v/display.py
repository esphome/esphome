import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_BACKLIGHT_PIN, CONF_BRIGHTNESS, CONF_CS_PIN, CONF_DC_PIN, \
                          CONF_HEIGHT, CONF_WIDTH, CONF_ID, CONF_LAMBDA, CONF_RESET_PIN
from . import st7789v_ns

CONF_OFFSET_HEIGHT = 'offset_height'
CONF_OFFSET_WIDTH = 'offset_width'

DEPENDENCIES = ['spi']

ST7789V = st7789v_ns.class_('ST7789V', cg.PollingComponent, spi.SPIDevice,
                            display.DisplayBuffer)
ST7789VRef = ST7789V.operator('ref')

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ST7789V),
    cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_CS_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_BACKLIGHT_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_BRIGHTNESS, default=1.0): cv.percentage,
    cv.Optional(CONF_HEIGHT, default=240): cv.int_,
    cv.Optional(CONF_WIDTH, default=135): cv.int_,
    cv.Optional(CONF_OFFSET_HEIGHT, default=52): cv.int_,
    cv.Optional(CONF_OFFSET_WIDTH, default=40): cv.int_,
}).extend(cv.polling_component_schema('5s')).extend(spi.spi_device_schema())


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset))

    bl = yield cg.gpio_pin_expression(config[CONF_BACKLIGHT_PIN])
    cg.add(var.set_backlight_pin(bl))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')], return_type=cg.void)
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_height(config[CONF_HEIGHT]))
    cg.add(var.set_width(config[CONF_WIDTH]))
    cg.add(var.set_offset_height(config[CONF_OFFSET_HEIGHT]))
    cg.add(var.set_offset_width(config[CONF_OFFSET_WIDTH]))

    yield display.register_display(var, config)
