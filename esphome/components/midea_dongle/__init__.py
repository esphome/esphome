import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID
from esphome.components.wifi_signal.sensor import WiFiSignalSensor

DEPENDENCIES = ['uart']
AUTO_LOAD = ['sensor', 'wifi_signal']
CODEOWNERS = ['@dudanov']

midea_dongle_ns = cg.esphome_ns.namespace('midea_dongle')
MideaDongle = midea_dongle_ns.class_('MideaDongle', cg.Component, uart.UARTDevice)

CONF_MIDEA_DONGLE_ID = 'midea_dongle_id'
CONF_WIFI_SIGNAL_ID = 'wifi_signal_id'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MideaDongle),
    cv.Optional(CONF_WIFI_SIGNAL_ID): cv.use_id(WiFiSignalSensor),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)
    if CONF_WIFI_SIGNAL_ID in config:
        ws = yield cg.get_variable(config[CONF_WIFI_SIGNAL_ID])
        cg.add(var.set_wifi_sensor(ws))
