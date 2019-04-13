from esphome.components import switch, uart
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_DATA, CONF_ID, CONF_INVERTED, CONF_NAME, CONF_UART_ID
from esphome.core import HexInt
from esphome.py_compat import text_type
from .. import uart_ns, UARTComponent

DEPENDENCIES = ['uart']

UARTSwitch = uart_ns.class_('UARTSwitch', switch.Switch, uart.UARTDevice)


def validate_data(value):
    if isinstance(value, text_type):
        return value.encode('utf-8')
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string wrapped in quotes or a list of bytes")


PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UARTSwitch),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    cv.Required(CONF_DATA): validate_data,
    cv.Optional(CONF_INVERTED): cv.invalid("UART switches do not support inverted mode!"),
}))


def to_code(config):
    uart_ = yield cg.get_variable(config[CONF_UART_ID])
    data = config[CONF_DATA]
    if isinstance(data, str):
        data = [HexInt(ord(x)) for x in data]
    var = cg.new_Pvariable(config[CONF_ID], uart_, config[CONF_NAME], data)
    yield cg.register_component(var, config)
    yield switch.register_switch(var, config)

