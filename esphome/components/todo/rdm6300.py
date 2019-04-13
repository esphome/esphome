from esphome.components import binary_sensor, uart
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_UART_ID


DEPENDENCIES = ['uart']

RDM6300Component = binary_sensor.binary_sensor_ns.class_('RDM6300Component', Component,
                                                         uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(RDM6300Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(uart.UARTComponent),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    uart_ = yield get_variable(config[CONF_UART_ID])
    rhs = App.make_rdm6300_component(uart_)
    var = Pvariable(config[CONF_ID], rhs)
    register_component(var, config)


BUILD_FLAGS = '-DUSE_RDM6300'
