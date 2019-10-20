import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ['uart']

gps_ns = cg.esphome_ns.namespace('gps')
GPS = gps_ns.class_('GPS', cg.Component, uart.UARTDevice)
GPSListener = gps_ns.class_('GPSListener')

CONF_GPS_ID = 'gps_id'
MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(GPS),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart.register_uart_device(var, config)

    # https://platformio.org/lib/show/1655/TinyGPSPlus
    cg.add_library('1655', '1.0.2')  # TinyGPSPlus, has name conflict
