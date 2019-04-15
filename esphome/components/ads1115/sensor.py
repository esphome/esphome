import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.components.ads1115 import ADS1115Component
from esphome.const import CONF_ADS1115_ID, CONF_GAIN, CONF_MULTIPLEXER, CONF_UPDATE_INTERVAL, \
    ICON_FLASH, UNIT_VOLT, CONF_ID, CONF_NAME
from esphome.py_compat import string_types
from . import ads1115_ns

DEPENDENCIES = ['ads1115']

ADS1115Multiplexer = ads1115_ns.enum('ADS1115Multiplexer')
MUX = {
    'A0_A1': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_N1,
    'A0_A3': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_N3,
    'A1_A3': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P1_N3,
    'A2_A3': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P2_N3,
    'A0_GND': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P0_NG,
    'A1_GND': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P1_NG,
    'A2_GND': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P2_NG,
    'A3_GND': ADS1115Multiplexer.ADS1115_MULTIPLEXER_P3_NG,
}

ADS1115Gain = ads1115_ns.enum('ADS1115Gain')
GAIN = {
    '6.144': ADS1115Gain.ADS1115_GAIN_6P144,
    '4.096': ADS1115Gain.ADS1115_GAIN_4P096,
    '2.048': ADS1115Gain.ADS1115_GAIN_2P048,
    '1.024': ADS1115Gain.ADS1115_GAIN_1P024,
    '0.512': ADS1115Gain.ADS1115_GAIN_0P512,
    '0.256': ADS1115Gain.ADS1115_GAIN_0P256,
}


def validate_gain(value):
    if isinstance(value, float):
        value = u'{:0.03f}'.format(value)
    elif not isinstance(value, string_types):
        raise cv.Invalid('invalid gain "{}"'.format(value))

    return cv.one_of(*GAIN)(value)


ADS1115Sensor = ads1115_ns.class_('ADS1115Sensor', sensor.Sensor)

CONFIG_SCHEMA = cv.nameable(sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 3).extend({
    cv.GenerateID(): cv.declare_variable_id(ADS1115Sensor),
    cv.GenerateID(CONF_ADS1115_ID): cv.use_variable_id(ADS1115Component),
    cv.Required(CONF_MULTIPLEXER): cv.one_of(*MUX, upper=True, space='_'),
    cv.Required(CONF_GAIN): validate_gain,
    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
}))


def to_code(config):
    hub = yield cg.get_variable(config[CONF_ADS1115_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_NAME], config[CONF_UPDATE_INTERVAL])
    cg.add(var.set_multiplexer(MUX[config[CONF_MULTIPLEXER]]))
    cg.add(var.set_gain(GAIN[config[CONF_GAIN]]))
    yield sensor.register_sensor(var, config)

    cg.add(hub.register_sensor(var))
