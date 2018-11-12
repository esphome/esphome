import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor, uart
from esphomeyaml.const import CONF_ID, CONF_UART_ID
from esphomeyaml.helpers import App, Pvariable, get_variable, setup_component, Component

DEPENDENCIES = ['uart']

RDM6300Component = binary_sensor.binary_sensor_ns.class_('RDM6300Component', Component,
                                                         uart.UARTDevice)

CONFIG_SCHEMA = vol.All(cv.ensure_list_not_empty, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RDM6300Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(uart.UARTComponent),
}).extend(cv.COMPONENT_SCHEMA.schema)])


def to_code(config):
    for conf in config:
        for uart_ in get_variable(conf[CONF_UART_ID]):
            yield
        rhs = App.make_rdm6300_component(uart_)
        var = Pvariable(conf[CONF_ID], rhs)
        setup_component(var, conf)


BUILD_FLAGS = '-DUSE_RDM6300'
