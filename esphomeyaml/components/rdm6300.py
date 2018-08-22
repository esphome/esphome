import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.uart import UARTComponent
from esphomeyaml.const import CONF_ID, CONF_UART_ID
from esphomeyaml.helpers import App, Pvariable, get_variable

DEPENDENCIES = ['uart']

RDM6300Component = binary_sensor.binary_sensor_ns.RDM6300Component

CONFIG_SCHEMA = vol.All(cv.ensure_list_not_empty, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RDM6300Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
})])


def to_code(config):
    for conf in config:
        uart = None
        for uart in get_variable(conf[CONF_UART_ID]):
            yield
        rhs = App.make_rdm6300_component(uart)
        Pvariable(conf[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_RDM6300'
