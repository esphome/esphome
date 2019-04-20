import voluptuous as vol

from esphome import pins
from esphome.components import display, spi
from esphome.components.spi import SPIComponent
import esphome.config_validation as cv
from esphome.const import CONF_CS_PIN, CONF_ID, CONF_INTENSITY, CONF_LAMBDA, CONF_NUM_CHIPS, \
    CONF_SPI_ID
from esphome.cpp_generator import Pvariable, add, get_variable, process_lambda
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, PollingComponent, void

DEPENDENCIES = ['spi']

MAX7219Component = display.display_ns.class_('MAX7219Component', PollingComponent, spi.SPIDevice)
MAX7219ComponentRef = MAX7219Component.operator('ref')

PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MAX7219Component),
    cv.GenerateID(CONF_SPI_ID): cv.use_variable_id(SPIComponent),
    vol.Required(CONF_CS_PIN): pins.gpio_output_pin_schema,

    vol.Optional(CONF_NUM_CHIPS): vol.All(cv.uint8_t, vol.Range(min=1)),
    vol.Optional(CONF_INTENSITY): vol.All(cv.uint8_t, vol.Range(min=0, max=15)),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    spi_ = yield get_variable(config[CONF_SPI_ID])
    cs = yield gpio_output_pin_expression(config[CONF_CS_PIN])
    rhs = App.make_max7219(spi_, cs)
    max7219 = Pvariable(config[CONF_ID], rhs)

    if CONF_NUM_CHIPS in config:
        add(max7219.set_num_chips(config[CONF_NUM_CHIPS]))
    if CONF_INTENSITY in config:
        add(max7219.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA], [(MAX7219ComponentRef, 'it')],
                                       return_type=void)
        add(max7219.set_writer(lambda_))

    display.setup_display(max7219, config)
    setup_component(max7219, config)


BUILD_FLAGS = '-DUSE_MAX7219'
