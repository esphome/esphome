import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_ADDRESS, CONF_START_CODE
from esphome.core import coroutine

DEPENDENCIES = ['uart']

rs485_ns = cg.esphome_ns.namespace('rs485')
RS485 = rs485_ns.class_('RS485', cg.Component, uart.UARTDevice)
RS485Device = rs485_ns.class_('RS485Device')
MULTI_CONF = True

CONF_RS485_ID = 'rs485_id'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RS485),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    cg.add_global(rs485_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    yield uart.register_uart_device(var, config)


def rs485_device_schema(start_code, address, crc):
    schema = {
        cv.GenerateID(CONF_RS485_ID): cv.use_id(RS485),
        cv.Required(CONF_START_CODE): cv.All([cv.uint8_t], cv.Length(min=1)),
        cv.Required(CONF_ADDRESS): cv.All([cv.uint8_t], cv.Length(min=1)),
    }
    if default_address is None:
        schema[cv.Required(CONF_ADDRESS)] = cv.All([cv.uint8_t], cv.Length(min=1)),
    else:
        schema[cv.Optional(CONF_ADDRESS, default=default_address)] = cv.hex_uint8_t
    return cv.Schema(schema)


@coroutine
def register_rs485_device(var, config):
    parent = yield cg.get_variable(config[CONF_RS485_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_start_code(config[CONF_START_CODE]))
    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(parent.register_device(var))
