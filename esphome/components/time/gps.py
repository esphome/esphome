import voluptuous as vol

from esphome.components import time as time_, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_UART_ID
from esphome.cpp_generator import Pvariable, add, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

DEPENDENCIES = ['uart']

GPSComponent = time_.time_ns.class_('GPSComponent', time_.RealTimeClockComponent, uart.UARTDevice)

PLATFORM_SCHEMA = time_.TIME_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPSComponent),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent)
}).extend(cv.COMPONENT_SCHEMA.schema)

def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield
    rhs = App.make_gps_component(uart_)
    gps = Pvariable(config[CONF_ID], rhs)
    time_.setup_time(gps, config)
    setup_component(gps, config)

BUILD_FLAGS = '-DUSE_GPS_COMPONENT'
