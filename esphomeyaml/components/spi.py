import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_CLK, CONF_ID, CONF_MISO, CONF_MOSI
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns, gpio_input_pin_expression, \
    gpio_output_pin_expression

SPIComponent = esphomelib_ns.SPIComponent

SPI_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(SPIComponent),
    vol.Required(CONF_CLK): pins.gpio_output_pin_schema,
    vol.Required(CONF_MISO): pins.gpio_input_pin_schema,
    vol.Optional(CONF_MOSI): pins.gpio_output_pin_schema,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [SPI_SCHEMA])


def to_code(config):
    for conf in config:
        clk = None
        for clk in gpio_output_pin_expression(conf[CONF_CLK]):
            yield
        miso = None
        for miso in gpio_input_pin_expression(conf[CONF_MISO]):
            yield
        mosi = None
        if CONF_MOSI in conf:
            for mosi in gpio_output_pin_expression(conf[CONF_MOSI]):
                yield
        rhs = App.init_spi(clk, miso, mosi)
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_SPI'
