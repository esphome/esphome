import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_BAUD_RATE, CONF_ID, CONF_RX, CONF_TX
from esphomeyaml.helpers import App, Pvariable, esphomelib_ns, gpio_input_pin_expression, \
    gpio_output_pin_expression

UARTComponent = esphomelib_ns.UARTComponent

SPI_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(UARTComponent),
    vol.Required(CONF_TX): pins.gpio_output_pin_schema,
    vol.Required(CONF_RX): pins.gpio_input_pin_schema,
    vol.Required(CONF_BAUD_RATE): cv.positive_int,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [SPI_SCHEMA])


def to_code(config):
    for conf in config:
        tx = None
        for tx in gpio_output_pin_expression(conf[CONF_TX]):
            yield
        rx = None
        for rx in gpio_input_pin_expression(conf[CONF_RX]):
            yield
        rhs = App.init_uart(tx, rx, conf[CONF_BAUD_RATE])
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_UART'
