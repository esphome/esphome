import voluptuous as vol

from esphome.components import switch, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
from esphome.const import CONF_DATA, CONF_ID, CONF_INVERTED, CONF_NAME, CONF_UART_ID
from esphome.core import HexInt
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_types import App
from esphome.py_compat import text_type

DEPENDENCIES = ['uart']

UARTSwitch = switch.switch_ns.class_('UARTSwitch', switch.Switch, uart.UARTDevice)


def validate_data(value):
    if isinstance(value, text_type):
        return value.encode('utf-8')
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return vol.Schema([cv.hex_uint8_t])(value)
    raise vol.Invalid("data must either be a string wrapped in quotes or a list of bytes")


PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UARTSwitch),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_DATA): validate_data,
    vol.Optional(CONF_INVERTED): cv.invalid("UART switches do not support inverted mode!"),
}))


def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield
    data = config[CONF_DATA]
    if isinstance(data, str):
        data = [HexInt(ord(x)) for x in data]
    rhs = App.make_uart_switch(uart_, config[CONF_NAME], data)
    var = Pvariable(config[CONF_ID], rhs)
    switch.setup_switch(var, config)


BUILD_FLAGS = '-DUSE_UART_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)
