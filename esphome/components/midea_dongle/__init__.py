import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']
CODEOWNERS = ['@dudanov']

midea_dongle_ns = cg.esphome_ns.namespace('midea_dongle')
MideaDongle = midea_dongle_ns.class_('MideaDongle', cg.Component, uart.UARTDevice)

CONF_MIDEA_DONGLE_ID = 'midea_dongle_id'
CONF_STRETCHED_ICON = 'stretched_icon'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MideaDongle),
    cv.Optional(CONF_STRETCHED_ICON): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    if CONF_STRETCHED_ICON in config:
        cg.add(var.use_stretched_icon(config[CONF_STRETCHED_ICON]))
