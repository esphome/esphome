import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADS1115_ID, CONF_GAIN, CONF_MULTIPLEXER, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import get_variable, RawExpression

DEPENDENCIES = ['ads1115']

MUX = {
    'A0_A1': 'ADS1115_MUX_P0_N1',
    'A0_A3': 'ADS1115_MUX_P0_N3',
    'A1_A3': 'ADS1115_MUX_P1_N3',
    'A2_A3': 'ADS1115_MUX_P2_N3',
    'A0_GND': 'ADS1115_MUX_P0_NG',
    'A1_GND': 'ADS1115_MUX_P1_NG',
    'A2_GND': 'ADS1115_MUX_P2_NG',
    'A3_GND': 'ADS1115_MUX_P3_NG',
}

GAIN = {
    '6.144': 'ADS1115_PGA_6P144',
    '4.096': 'ADS1115_PGA_6P096',
    '2.048': 'ADS1115_PGA_2P048',
    '1.024': 'ADS1115_PGA_1P024',
    '0.512': 'ADS1115_PGA_0P512',
    '0.256': 'ADS1115_PGA_0P256',
}


def validate_gain(value):
    if isinstance(value, float):
        value = u'{:0.03f}'.format(value)
    elif not isinstance(value, (str, unicode)):
        raise vol.Invalid('invalid gain "{}"'.format(value))

    if value not in GAIN:
        raise vol.Invalid("Invalid gain, options are {}".format(', '.join(GAIN.keys())))
    return value


PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    vol.Required(CONF_MULTIPLEXER): vol.All(vol.Upper, vol.Any(*list(MUX.keys()))),
    vol.Required(CONF_GAIN): validate_gain,
    vol.Optional(CONF_ADS1115_ID): cv.variable_id,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_not_null_time_period,
}).extend(sensor.MQTT_SENSOR_ID_SCHEMA.schema)


def to_code(config):
    hub = get_variable(config.get(CONF_ADS1115_ID), u'sensor::ADS1115Component')

    mux = RawExpression(MUX[config[CONF_MULTIPLEXER]])
    gain = RawExpression(GAIN[config[CONF_GAIN]])
    sensor_ = hub.get_sensor(mux, gain, config.get(CONF_UPDATE_INTERVAL))
    sensor.make_mqtt_sensor_for(sensor_, config)
