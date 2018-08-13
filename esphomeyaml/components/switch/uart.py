import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.components.uart import UARTComponent
from esphomeyaml.const import CONF_DATA, CONF_INVERTED, CONF_MAKE_ID, CONF_NAME, CONF_UART_ID
from esphomeyaml.core import HexInt
from esphomeyaml.helpers import App, Application, ArrayInitializer, get_variable, variable

DEPENDENCIES = ['uart']

MakeUARTSwitch = Application.MakeUARTSwitch


def validate_data(value):
    if isinstance(value, unicode):
        return value.encode('utf-8')
    elif isinstance(value, str):
        return value
    elif isinstance(value, list):
        return vol.Schema([cv.hex_uint8_t])(value)
    raise vol.Invalid("data must either be a string wrapped in quotes or a list of bytes")


PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeUARTSwitch),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_DATA): validate_data,
    vol.Optional(CONF_INVERTED): cv.invalid("UART switches do not support inverted mode!"),
}))


def to_code(config):
    uart = None
    for uart in get_variable(config[CONF_UART_ID]):
        yield
    data = config[CONF_DATA]
    if isinstance(data, str):
        data = [HexInt(ord(x)) for x in data]
    rhs = App.make_uart_switch(uart, config[CONF_NAME], ArrayInitializer(*data, multiline=False))
    restart = variable(config[CONF_MAKE_ID], rhs)
    switch.setup_switch(restart.Puart, restart.Pmqtt, config)


BUILD_FLAGS = '-DUSE_UART_SWITCH'
