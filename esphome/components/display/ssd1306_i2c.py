import voluptuous as vol

from esphome import pins
from esphome.components import display
from esphome.components.display import ssd1306_spi
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_EXTERNAL_VCC, CONF_ID, CONF_LAMBDA, CONF_MODEL, \
    CONF_PAGES, CONF_RESET_PIN
from esphome.cpp_generator import Pvariable, add, process_lambda
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, void

DEPENDENCIES = ['i2c']

I2CSSD1306 = display.display_ns.class_('I2CSSD1306', ssd1306_spi.SSD1306)

PLATFORM_SCHEMA = vol.All(display.FULL_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(I2CSSD1306),
    vol.Required(CONF_MODEL): ssd1306_spi.SSD1306_MODEL,
    vol.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_EXTERNAL_VCC): cv.boolean,
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))


def to_code(config):
    ssd = Pvariable(config[CONF_ID], App.make_i2c_ssd1306())
    add(ssd.set_model(ssd1306_spi.MODELS[config[CONF_MODEL]]))

    if CONF_RESET_PIN in config:
        for reset in gpio_output_pin_expression(config[CONF_RESET_PIN]):
            yield
        add(ssd.set_reset_pin(reset))
    if CONF_EXTERNAL_VCC in config:
        add(ssd.set_external_vcc(config[CONF_EXTERNAL_VCC]))
    if CONF_ADDRESS in config:
        add(ssd.set_address(config[CONF_ADDRESS]))
    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA],
                                      [(display.DisplayBufferRef, 'it')], return_type=void):
            yield
        add(ssd.set_writer(lambda_))

    display.setup_display(ssd, config)
    setup_component(ssd, config)


BUILD_FLAGS = '-DUSE_SSD1306'
