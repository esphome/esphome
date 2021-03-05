import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_DC_PIN, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES, CONF_RESET_PIN

DEPENDENCIES = ['spi']

ili9486_ns = cg.esphome_ns.namespace('ili9486')
ili9486 = ili9486_ns.class_('ILI9486Display', cg.PollingComponent, spi.SPIDevice,
                            display.DisplayBuffer)
ILI9486TFT35 = ili9486_ns.class_('ILI9486TFT35', ili9486)

ILI9486Model = ili9486_ns.enum('ILI9486Model')

MODELS = {
    'TFT_3.5': ILI9486Model.TFT_35,
}

ILI9486_MODEL = cv.enum(MODELS, upper=True, space="_")

CONFIG_SCHEMA = cv.All(display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ili9486),
    cv.Required(CONF_MODEL): ILI9486_MODEL,
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
}).extend(cv.polling_component_schema('1s')).extend(spi.spi_device_schema()),
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    if config[CONF_MODEL] == 'TFT_3.5':
        lcd_type = ILI9486TFT35
    rhs = lcd_type.new()
    var = cg.Pvariable(config[CONF_ID], rhs)

    yield cg.register_component(var, config)
    yield display.register_display(var, config)
    yield spi.register_spi_device(var, config)
    cg.add(var.set_model(config[CONF_MODEL]))
    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
