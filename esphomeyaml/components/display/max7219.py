import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import display
from esphomeyaml.components.spi import SPIComponent
from esphomeyaml.const import CONF_CS_PIN, CONF_ID, CONF_INTENSITY, CONF_LAMBDA, CONF_NUM_CHIPS, \
    CONF_SPI_ID
from esphomeyaml.helpers import App, Pvariable, add, get_variable, gpio_output_pin_expression, \
    process_lambda

DEPENDENCIES = ['spi']

MAX7219Component = display.display_ns.MAX7219Component
MAX7219ComponentRef = MAX7219Component.operator('ref')

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MAX7219Component),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,

    vol.Optional(CONF_NUM_CHIPS): vol.All(cv.uint8_t, vol.Range(min=1)),
    vol.Optional(CONF_INTENSITY): vol.All(cv.uint8_t, vol.Range(min=0, max=15)),
})


def to_code(config):
    for spi in get_variable(config[CONF_SPI_ID]):
        yield
    for cs in gpio_output_pin_expression(config[CONF_CS_PIN]):
        yield
    rhs = App.make_max7219(spi, cs)
    max7219 = Pvariable(config[CONF_ID], rhs)

    if CONF_NUM_CHIPS in config:
        add(max7219.set_num_chips(config[CONF_NUM_CHIPS]))
    if CONF_INTENSITY in config:
        add(max7219.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(MAX7219ComponentRef, 'it')]):
            yield
        add(max7219.set_writer(lambda_))

    display.setup_display(max7219, config)


BUILD_FLAGS = '-DUSE_MAX7219'
