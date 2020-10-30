from esphome.components import time
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']

tuya_ns = cg.esphome_ns.namespace('tuya')
Tuya = tuya_ns.class_('Tuya', cg.Component, uart.UARTDevice)

CONF_TUYA_ID = 'tuya_id'
CONF_CLOCK = 'clock'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Tuya),
    cv.Optional(CONF_CLOCK): cv.use_id(time.RealTimeClock),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    if CONF_CLOCK in config:
        time_ = yield cg.get_variable(config[CONF_CLOCK])
        cg.add(var.set_clock(time_))
