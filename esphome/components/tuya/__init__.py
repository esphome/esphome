import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']

tuya_ns = cg.esphome_ns.namespace('tuya')
Tuya = tuya_ns.class_('Tuya', cg.Component, uart.UARTDevice)

CONF_TUYA_ID = 'tuya_id'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Tuya),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
