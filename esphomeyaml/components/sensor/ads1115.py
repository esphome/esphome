import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.components.ads1115 import ADS1115Component
from esphomeyaml.const import CONF_ADS1115_ID, CONF_GAIN, CONF_MULTIPLEXER, CONF_NAME, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import get_variable

DEPENDENCIES = ['ads1115']

MUX = {
    'A0_A1': sensor.sensor_ns.ADS1115_MULTIPLEXER_P0_N1,
    'A0_A3': sensor.sensor_ns.ADS1115_MULTIPLEXER_P0_N3,
    'A1_A3': sensor.sensor_ns.ADS1115_MULTIPLEXER_P1_N3,
    'A2_A3': sensor.sensor_ns.ADS1115_MULTIPLEXER_P2_N3,
    'A0_GND': sensor.sensor_ns.ADS1115_MULTIPLEXER_P0_NG,
    'A1_GND': sensor.sensor_ns.ADS1115_MULTIPLEXER_P1_NG,
    'A2_GND': sensor.sensor_ns.ADS1115_MULTIPLEXER_P2_NG,
    'A3_GND': sensor.sensor_ns.ADS1115_MULTIPLEXER_P3_NG,
}

GAIN = {
    '6.144': sensor.sensor_ns.ADS1115_GAIN_6P144,
    '4.096': sensor.sensor_ns.ADS1115_GAIN_6P096,
    '2.048': sensor.sensor_ns.ADS1115_GAIN_2P048,
    '1.024': sensor.sensor_ns.ADS1115_GAIN_1P024,
    '0.512': sensor.sensor_ns.ADS1115_GAIN_0P512,
    '0.256': sensor.sensor_ns.ADS1115_GAIN_0P256,
}


def validate_gain(value):
    if isinstance(value, float):
        value = u'{:0.03f}'.format(value)
    elif not isinstance(value, (str, unicode)):
        raise vol.Invalid('invalid gain "{}"'.format(value))

    return cv.one_of(*GAIN)(value)


def validate_mux(value):
    value = cv.string(value).upper()
    value = value.replace(' ', '_')
    return cv.one_of(*MUX)(value)


PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_MULTIPLEXER): validate_mux,
    vol.Required(CONF_GAIN): validate_gain,
    cv.GenerateID(CONF_ADS1115_ID): cv.use_variable_id(ADS1115Component),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ADS1115_ID]):
        yield

    mux = MUX[config[CONF_MULTIPLEXER]]
    gain = GAIN[config[CONF_GAIN]]
    rhs = hub.get_sensor(config[CONF_NAME], mux, gain, config.get(CONF_UPDATE_INTERVAL))
    sensor.register_sensor(rhs, config)


BUILD_FLAGS = '-DUSE_ADS1115_SENSOR'
