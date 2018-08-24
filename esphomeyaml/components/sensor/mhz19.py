import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.uart import UARTComponent
from esphomeyaml.const import CONF_CO2, CONF_MAKE_ID, CONF_NAME, CONF_TEMPERATURE, CONF_UART_ID, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, get_variable, variable

DEPENDENCIES = ['uart']

MakeMHZ19Sensor = Application.MakeMHZ19Sensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeMHZ19Sensor),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_CO2): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
})


def to_code(config):
    uart = None
    for uart in get_variable(config[CONF_UART_ID]):
        yield
    rhs = App.make_mhz19_sensor(uart, config[CONF_CO2][CONF_NAME],
                                config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    mhz19 = make.Pmhz19
    sensor.setup_sensor(mhz19.Pget_co2_sensor(), make.Pmqtt, config[CONF_CO2])

    if CONF_TEMPERATURE in config:
        sensor.register_sensor(mhz19.Pmake_temperature_sensor(config[CONF_TEMPERATURE][CONF_NAME]),
                               config[CONF_TEMPERATURE])


BUILD_FLAGS = '-DUSE_MHZ19'
