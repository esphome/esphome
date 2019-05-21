import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, uart
from esphome.const import CONF_DATA, CONF_ID, CONF_INVERTED
from esphome.core import HexInt
from esphome.py_compat import text_type, binary_type, char_to_byte
from .. import uart_ns

DEPENDENCIES = ['uart']

UARTSwitch = uart_ns.class_('UARTSwitch', switch.Switch, uart.UARTDevice, cg.Component)


def validate_data(value):
    if isinstance(value, text_type):
        return value.encode('utf-8')
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string wrapped in quotes or a list of bytes")


CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(UARTSwitch),
    cv.Required(CONF_DATA): validate_data,
    cv.Optional(CONF_INVERTED): cv.invalid("UART switches do not support inverted mode!"),
}).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)
    yield uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    if isinstance(data, binary_type):
        data = [HexInt(char_to_byte(x)) for x in data]
    cg.add(var.set_data(data))
