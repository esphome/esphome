import voluptuous as vol

from esphomeyaml.components import sensor
from esphomeyaml.components.uart import UARTComponent
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_FORMALDEHYDE, CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_PM_10_0, \
    CONF_PM_1_0, CONF_PM_2_5, CONF_TEMPERATURE, CONF_TYPE, CONF_UART_ID
from esphomeyaml.helpers import App, Pvariable, get_variable

DEPENDENCIES = ['uart']

PMSX003Component = sensor.sensor_ns.PMSX003Component

CONF_PMSX003 = 'PMSX003'
CONF_PMS5003T = 'PMS5003T'
CONF_PMS5003ST = 'PMS5003ST'

PMSX003_TYPES = {
    CONF_PMSX003: sensor.sensor_ns.PMSX003_TYPE_X003,
    CONF_PMS5003T: sensor.sensor_ns.PMSX003_TYPE_5003T,
    CONF_PMS5003ST: sensor.sensor_ns.PMSX003_TYPE_5003ST,
}

SENSORS_TO_TYPE = {
    CONF_PM_1_0: [CONF_PMSX003],
    CONF_PM_2_5: [CONF_PMSX003, CONF_PMS5003T, CONF_PMS5003ST],
    CONF_PM_10_0: [CONF_PMSX003],
    CONF_TEMPERATURE: [CONF_PMS5003T, CONF_PMS5003ST],
    CONF_HUMIDITY: [CONF_PMS5003T, CONF_PMS5003ST],
    CONF_FORMALDEHYDE: [CONF_PMS5003ST],
}


def validate_pmsx003_sensors(value):
    for key, types in SENSORS_TO_TYPE.iteritems():
        if key in value and value[CONF_TYPE] not in types:
            raise vol.Invalid(u"{} does not have {} sensor!".format(value[CONF_TYPE], key))
    return value


PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PMSX003Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_TYPE): vol.All(vol.Upper, cv.one_of(*PMSX003_TYPES)),

    vol.Optional(CONF_PM_1_0): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_PM_2_5): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_PM_10_0): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_HUMIDITY): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_FORMALDEHYDE): cv.nameable(sensor.SENSOR_SCHEMA),
}), cv.has_at_least_one_key(*SENSORS_TO_TYPE))


def to_code(config):
    for uart in get_variable(config[CONF_UART_ID]):
        yield

    rhs = App.make_pmsx003(uart, PMSX003_TYPES[config[CONF_TYPE]])
    pms = Pvariable(config[CONF_ID], rhs)

    if CONF_PM_1_0 in config:
        conf = config[CONF_PM_1_0]
        sensor.register_sensor(pms.make_pm_1_0_sensor(conf[CONF_NAME]), conf)

    if CONF_PM_2_5 in config:
        conf = config[CONF_PM_2_5]
        sensor.register_sensor(pms.make_pm_2_5_sensor(conf[CONF_NAME]), conf)

    if CONF_PM_10_0 in config:
        conf = config[CONF_PM_10_0]
        sensor.register_sensor(pms.make_pm_10_0_sensor(conf[CONF_NAME]), conf)

    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        sensor.register_sensor(pms.make_temperature_sensor(conf[CONF_NAME]), conf)

    if CONF_HUMIDITY in config:
        conf = config[CONF_HUMIDITY]
        sensor.register_sensor(pms.make_humidity_sensor(conf[CONF_NAME]), conf)

    if CONF_FORMALDEHYDE in config:
        conf = config[CONF_FORMALDEHYDE]
        sensor.register_sensor(pms.make_formaldehyde_sensor(conf[CONF_NAME]), conf)


BUILD_FLAGS = '-DUSE_PMSX003'
