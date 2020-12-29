import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
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


RS485_DEVICE_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_RS485_ID): cv.use_id(RS485),
})


@coroutine
def register_rs485_device(var, config):
    parent = yield cg.get_variable(config[CONF_RS485_ID])
    cg.add(var.set_parent(parent))
    cg.add(parent.register_device(var))
