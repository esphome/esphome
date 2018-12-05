import voluptuous as vol

from esphomeyaml import pins
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_BAUD_RATE, CONF_ID, CONF_RX_PIN, CONF_TX_PIN
from esphomeyaml.cpp_generator import Pvariable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Component, esphomelib_ns

UARTComponent = esphomelib_ns.class_('UARTComponent', Component)
UARTDevice = esphomelib_ns.class_('UARTDevice')
MULTI_CONF = True

CONFIG_SCHEMA = vol.All(vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(UARTComponent),
    vol.Optional(CONF_TX_PIN): pins.output_pin,
    vol.Optional(CONF_RX_PIN): pins.input_pin,
    vol.Required(CONF_BAUD_RATE): cv.positive_int,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(CONF_TX_PIN, CONF_RX_PIN))


def to_code(config):
    tx = config.get(CONF_TX_PIN, -1)
    rx = config.get(CONF_RX_PIN, -1)
    rhs = App.init_uart(tx, rx, config[CONF_BAUD_RATE])
    var = Pvariable(config[CONF_ID], rhs)

    setup_component(var, config)


BUILD_FLAGS = '-DUSE_UART'
