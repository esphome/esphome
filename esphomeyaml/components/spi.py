import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_CLK_PIN, CONF_ID, CONF_MISO_PIN, CONF_MOSI_PIN
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns, gpio_input_pin_expression, \
    gpio_output_pin_expression, add

SPIComponent = esphomelib_ns.SPIComponent

SPI_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(SPIComponent),
    vol.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_MISO_PIN): pins.gpio_input_pin_schema,
    vol.Optional(CONF_MOSI_PIN): pins.gpio_output_pin_schema,
}), cv.has_at_least_one_key(CONF_MISO_PIN, CONF_MOSI_PIN))

CONFIG_SCHEMA = vol.All(cv.ensure_list, [SPI_SCHEMA])


def to_code(config):
    for conf in config:
        clk = None
        for clk in gpio_output_pin_expression(conf[CONF_CLK_PIN]):
            yield
        rhs = App.init_spi(clk)
        spi = Pvariable(conf[CONF_ID], rhs)
        if CONF_MISO_PIN in conf:
            for miso in gpio_input_pin_expression(conf[CONF_MISO_PIN]):
                yield
            add(spi.set_miso(miso))
        if CONF_MOSI_PIN in conf:
            for mosi in gpio_input_pin_expression(conf[CONF_MOSI_PIN]):
                yield
            add(spi.set_mosi(mosi))


BUILD_FLAGS = '-DUSE_SPI'
