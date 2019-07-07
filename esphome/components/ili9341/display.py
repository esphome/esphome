import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display, spi
from esphome.const import CONF_DC_PIN, CONF_FULL_UPDATE_EVERY, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_PAGES, CONF_RESET_PIN

DEPENDENCIES = ['spi']

ili9341_ns = cg.esphome_ns.namespace('ili9341')
ili9341 = ili9341_ns.class_('ILI9341', cg.PollingComponent, spi.SPIDevice,
                                             display.DisplayBuffer)
ILI9341M5Stack = ili9341_ns.class_('ili9341_M5Stack', ili9341)


def validate_full_update_every_only_type_a(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    return value


CONFIG_SCHEMA = cv.All(display.FULL_DISPLAY_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(ili9341),
    cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_FULL_UPDATE_EVERY): cv.uint32_t,
}).extend(cv.polling_component_schema('1s')).extend(spi.SPI_DEVICE_SCHEMA),
                       validate_full_update_every_only_type_a,
                       cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    rhs = ILI9341M5Stack.new()
    var = cg.Pvariable(config[CONF_ID], rhs, type=ILI9341M5Stack)

    yield cg.register_component(var, config)
    yield display.register_display(var, config)
    yield spi.register_spi_device(var, config)

    dc = yield cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc))

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        reset = yield cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        cg.add(var.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))
