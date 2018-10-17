import voluptuous as vol

from esphomeyaml.components import sensor
from esphomeyaml.components.uart import UARTComponent
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_CURRENT, CONF_ID, CONF_NAME, CONF_POWER, CONF_UART_ID, \
    CONF_VOLTAGE
from esphomeyaml.helpers import App, Pvariable, get_variable

DEPENDENCIES = ['uart']

CSE7766Component = sensor.sensor_ns.CSE7766Component

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CSE7766Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),

    vol.Optional(CONF_VOLTAGE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_CURRENT): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_POWER): cv.nameable(sensor.SENSOR_SCHEMA),
}), cv.has_at_least_one_key(CONF_VOLTAGE, CONF_CURRENT, CONF_POWER))


def to_code(config):
    for uart in get_variable(config[CONF_UART_ID]):
        yield

    rhs = App.make_cse7766(uart)
    cse = Pvariable(config[CONF_ID], rhs)

    if CONF_VOLTAGE in config:
        conf = config[CONF_VOLTAGE]
        sensor.register_sensor(cse.make_voltage_sensor(conf[CONF_NAME]), conf)
    if CONF_CURRENT in config:
        conf = config[CONF_CURRENT]
        sensor.register_sensor(cse.make_current_sensor(conf[CONF_NAME]), conf)
    if CONF_POWER in config:
        conf = config[CONF_POWER]
        sensor.register_sensor(cse.make_power_sensor(conf[CONF_NAME]), conf)


BUILD_FLAGS = '-DUSE_CSE7766'
