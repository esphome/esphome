import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import display
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_BUSY_PIN, CONF_CS_PIN, CONF_DC_PIN, CONF_FULL_UPDATE_EVERY, \
    CONF_ID, CONF_LAMBDA, CONF_MODEL, CONF_RESET_PIN, CONF_SPI_ID
from esphomeyaml.helpers import App, Pvariable, add, get_variable, gpio_input_pin_expression, \
    gpio_output_pin_expression, process_lambda

DEPENDENCIES = ['spi']

WaveshareEPaperTypeA = display.display_ns.WaveshareEPaperTypeA
WaveshareEPaper = display.display_ns.WaveshareEPaper

MODELS = {
    '1.54in': ('a', display.display_ns.WAVESHARE_EPAPER_1_54_IN),
    '2.13in': ('a', display.display_ns.WAVESHARE_EPAPER_2_13_IN),
    '2.90in': ('a', display.display_ns.WAVESHARE_EPAPER_2_9_IN),
    '2.70in': ('b', display.display_ns.WAVESHARE_EPAPER_2_7_IN),
    '4.20in': ('b', display.display_ns.WAVESHARE_EPAPER_4_2_IN),
    '7.50in': ('b', display.display_ns.WAVESHARE_EPAPER_7_5_IN),
}


def validate_full_update_every_only_type_a(value):
    if CONF_FULL_UPDATE_EVERY not in value:
        return value
    if MODELS[value[CONF_MODEL]][0] != 'a':
        raise vol.Invalid("The 'full_update_every' option is only available for models "
                          "'1.54in', '2.13in' and '2.90in'.")
    return value


PLATFORM_SCHEMA = vol.All(display.FULL_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(None),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_MODEL): vol.All(vol.Lower, cv.one_of(*MODELS)),
    vol.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
    vol.Optional(CONF_FULL_UPDATE_EVERY): cv.uint32_t,
}), validate_full_update_every_only_type_a)


def to_code(config):
    for spi in get_variable(config[CONF_SPI_ID]):
        yield
    for cs in gpio_output_pin_expression(config[CONF_CS_PIN]):
        yield
    for dc in gpio_output_pin_expression(config[CONF_DC_PIN]):
        yield

    model_type, model = MODELS[config[CONF_MODEL]]
    if model_type == 'a':
        rhs = App.make_waveshare_epaper_type_a(spi, cs, dc, model)
        epaper = Pvariable(config[CONF_ID], rhs, type=WaveshareEPaperTypeA)
    elif model_type == 'b':
        rhs = App.make_waveshare_epaper_type_b(spi, cs, dc, model)
        epaper = Pvariable(config[CONF_ID], rhs, type=WaveshareEPaper)
    else:
        raise NotImplementedError()

    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')]):
            yield
        add(epaper.set_writer(lambda_))
    if CONF_RESET_PIN in config:
        for reset in gpio_output_pin_expression(config[CONF_RESET_PIN]):
            yield
        add(epaper.set_reset_pin(reset))
    if CONF_BUSY_PIN in config:
        for reset in gpio_input_pin_expression(config[CONF_BUSY_PIN]):
            yield
        add(epaper.set_busy_pin(reset))
    if CONF_FULL_UPDATE_EVERY in config:
        add(epaper.set_full_update_every(config[CONF_FULL_UPDATE_EVERY]))

    display.setup_display(epaper, config)


BUILD_FLAGS = '-DUSE_WAVESHARE_EPAPER'
