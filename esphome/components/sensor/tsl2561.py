import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_GAIN, CONF_ID, CONF_INTEGRATION_TIME, CONF_NAME, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

DEPENDENCIES = ['i2c']

TSL2561IntegrationTime = sensor.sensor_ns.enum('TSL2561IntegrationTime')
INTEGRATION_TIMES = {
    14: TSL2561IntegrationTime.TSL2561_INTEGRATION_14MS,
    101: TSL2561IntegrationTime.TSL2561_INTEGRATION_101MS,
    402: TSL2561IntegrationTime.TSL2561_INTEGRATION_402MS,
}

TSL2561Gain = sensor.sensor_ns.enum('TSL2561Gain')
GAINS = {
    '1X': TSL2561Gain.TSL2561_GAIN_1X,
    '16X': TSL2561Gain.TSL2561_GAIN_16X,
}

CONF_IS_CS_PACKAGE = 'is_cs_package'


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    if value not in INTEGRATION_TIMES:
        raise vol.Invalid(u"Unsupported integration time {}.".format(value))
    return value


TSL2561Sensor = sensor.sensor_ns.class_('TSL2561Sensor', sensor.PollingSensorComponent,
                                        i2c.I2CDevice)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TSL2561Sensor),
    vol.Optional(CONF_ADDRESS, default=0x39): cv.i2c_address,
    vol.Optional(CONF_INTEGRATION_TIME): validate_integration_time,
    vol.Optional(CONF_GAIN): cv.one_of(*GAINS, upper=True),
    vol.Optional(CONF_IS_CS_PACKAGE): cv.boolean,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_tsl2561_sensor(config[CONF_NAME], config[CONF_ADDRESS],
                                  config.get(CONF_UPDATE_INTERVAL))
    tsl2561 = Pvariable(config[CONF_ID], rhs)

    if CONF_INTEGRATION_TIME in config:
        add(tsl2561.set_integration_time(INTEGRATION_TIMES[config[CONF_INTEGRATION_TIME]]))
    if CONF_GAIN in config:
        add(tsl2561.set_gain(GAINS[config[CONF_GAIN]]))
    if CONF_IS_CS_PACKAGE in config:
        add(tsl2561.set_is_cs_package(config[CONF_IS_CS_PACKAGE]))

    sensor.setup_sensor(tsl2561, config)
    setup_component(tsl2561, config)


BUILD_FLAGS = '-DUSE_TSL2561'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
