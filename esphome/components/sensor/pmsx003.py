import voluptuous as vol

from esphome.components import sensor, uart
from esphome.components.uart import UARTComponent
import esphome.config_validation as cv
from esphome.const import CONF_FORMALDEHYDE, CONF_HUMIDITY, CONF_ID, CONF_NAME, CONF_PM_10_0, \
    CONF_PM_1_0, CONF_PM_2_5, CONF_TEMPERATURE, CONF_TYPE, CONF_UART_ID
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Component

DEPENDENCIES = ['uart']

PMSX003Component = sensor.sensor_ns.class_('PMSX003Component', uart.UARTDevice, Component)
PMSX003Sensor = sensor.sensor_ns.class_('PMSX003Sensor', sensor.Sensor)

CONF_PMSX003 = 'PMSX003'
CONF_PMS5003T = 'PMS5003T'
CONF_PMS5003ST = 'PMS5003ST'

PMSX003Type = sensor.sensor_ns.enum('PMSX003Type')
PMSX003_TYPES = {
    CONF_PMSX003: PMSX003Type.PMSX003_TYPE_X003,
    CONF_PMS5003T: PMSX003Type.PMSX003_TYPE_5003T,
    CONF_PMS5003ST: PMSX003Type.PMSX003_TYPE_5003ST,
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
    for key, types in SENSORS_TO_TYPE.items():
        if key in value and value[CONF_TYPE] not in types:
            raise vol.Invalid(u"{} does not have {} sensor!".format(value[CONF_TYPE], key))
    return value


PMSX003_SENSOR_SCHEMA = sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PMSX003Sensor),
})

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(PMSX003Component),
    cv.GenerateID(CONF_UART_ID): cv.use_variable_id(UARTComponent),
    vol.Required(CONF_TYPE): cv.one_of(*PMSX003_TYPES, upper=True),

    vol.Optional(CONF_PM_1_0): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_PM_2_5): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_PM_10_0): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_HUMIDITY): cv.nameable(PMSX003_SENSOR_SCHEMA),
    vol.Optional(CONF_FORMALDEHYDE): cv.nameable(PMSX003_SENSOR_SCHEMA),
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSORS_TO_TYPE))


def to_code(config):
    for uart_ in get_variable(config[CONF_UART_ID]):
        yield

    rhs = App.make_pmsx003(uart_, PMSX003_TYPES[config[CONF_TYPE]])
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

    setup_component(pms, config)


BUILD_FLAGS = '-DUSE_PMSX003'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_PM_1_0, CONF_PM_2_5, CONF_PM_10_0, CONF_TEMPERATURE, CONF_HUMIDITY,
                CONF_FORMALDEHYDE):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
