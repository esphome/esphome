import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_CLK_PIN, CONF_ID, CONF_MISO_PIN, CONF_MOSI_PIN
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, gpio_output_pin_expression, \
    setup_component
from esphome.cpp_types import App, Component, esphome_ns

SPIComponent = esphome_ns.class_('SPIComponent', Component)
SPIDevice = esphome_ns.class_('SPIDevice')
MULTI_CONF = True

CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(SPIComponent),
    vol.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
    vol.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
}), cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN))


def to_code(config):
    for clk in gpio_output_pin_expression(config[CONF_CLK_PIN]):
        yield
    rhs = App.init_spi(clk)
    spi = Pvariable(config[CONF_ID], rhs)
    if CONF_MISO_PIN in config:
        for miso in gpio_input_pin_expression(config[CONF_MISO_PIN]):
            yield
        add(spi.set_miso(miso))
    if CONF_MOSI_PIN in config:
        for mosi in gpio_input_pin_expression(config[CONF_MOSI_PIN]):
            yield
        add(spi.set_mosi(mosi))

    setup_component(spi, config)


BUILD_FLAGS = '-DUSE_SPI'
