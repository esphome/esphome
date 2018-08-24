import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_GAIN, CONF_INTEGRATION_TIME, CONF_MAKE_ID, \
    CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, variable

DEPENDENCIES = ['i2c']

INTEGRATION_TIMES = {
    14: sensor.sensor_ns.TSL2561_INTEGRATION_14MS,
    101: sensor.sensor_ns.TSL2561_INTEGRATION_101MS,
    402: sensor.sensor_ns.TSL2561_INTEGRATION_402MS,
}
GAINS = {
    '1X': sensor.sensor_ns.TSL2561_GAIN_1X,
    '16X': sensor.sensor_ns.TSL2561_GAIN_16X,
}

CONF_IS_CS_PACKAGE = 'is_cs_package'


def validate_integration_time(value):
    value = cv.positive_time_period_milliseconds(value).total_milliseconds
    if value not in INTEGRATION_TIMES:
        raise vol.Invalid(u"Unsupported integration time {}.".format(value))
    return value


MakeTSL2561Sensor = Application.MakeTSL2561Sensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTSL2561Sensor),
    vol.Optional(CONF_ADDRESS, default=0x39): cv.i2c_address,
    vol.Optional(CONF_INTEGRATION_TIME): validate_integration_time,
    vol.Optional(CONF_GAIN): vol.All(vol.Upper, cv.one_of(*GAINS)),
    vol.Optional(CONF_IS_CS_PACKAGE): cv.boolean,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    rhs = App.make_tsl2561_sensor(config[CONF_NAME], config[CONF_ADDRESS],
                                  config.get(CONF_UPDATE_INTERVAL))
    make_tsl = variable(config[CONF_MAKE_ID], rhs)
    tsl2561 = make_tsl.Ptsl2561
    if CONF_INTEGRATION_TIME in config:
        add(tsl2561.set_integration_time(INTEGRATION_TIMES[config[CONF_INTEGRATION_TIME]]))
    if CONF_GAIN in config:
        add(tsl2561.set_gain(GAINS[config[CONF_GAIN]]))
    if CONF_IS_CS_PACKAGE in config:
        add(tsl2561.set_is_cs_package(config[CONF_IS_CS_PACKAGE]))
    sensor.setup_sensor(tsl2561, make_tsl.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TSL2561'
