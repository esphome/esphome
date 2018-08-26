# coding=utf-8
import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_UPDATE_INTERVAL, CONF_RANGE
from esphomeyaml.helpers import App, Pvariable, add

DEPENDENCIES = ['i2c']

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'
CONF_HEADING = 'heading'

HMC5883LComponent = sensor.sensor_ns.HMC5883LComponent
HMC5883LFieldStrengthSensor = sensor.sensor_ns.HMC5883LFieldStrengthSensor
HMC5883LHeadingSensor = sensor.sensor_ns.HMC5883LHeadingSensor

HMC5883L_RANGES = {
    88: sensor.sensor_ns.HMC5883L_RANGE_88_UT,
    130: sensor.sensor_ns.HMC5883L_RANGE_130_UT,
    190: sensor.sensor_ns.HMC5883L_RANGE_190_UT,
    250: sensor.sensor_ns.HMC5883L_RANGE_250_UT,
    400: sensor.sensor_ns.HMC5883L_RANGE_400_UT,
    470: sensor.sensor_ns.HMC5883L_RANGE_470_UT,
    560: sensor.sensor_ns.HMC5883L_RANGE_560_UT,
    810: sensor.sensor_ns.HMC5883L_RANGE_810_UT,
}


def validate_range(value):
    value = cv.string(value)
    if value.endswith(u'µT') or value.endswith('uT'):
        value = value[:-2]
    return cv.one_of(*HMC5883L_RANGES)(int(value))


PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HMC5883LComponent),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_FIELD_STRENGTH_X): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_FIELD_STRENGTH_Y): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_FIELD_STRENGTH_Z): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_HEADING): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_RANGE): validate_range,
}), cv.has_at_least_one_key(CONF_FIELD_STRENGTH_X, CONF_FIELD_STRENGTH_Y, CONF_FIELD_STRENGTH_Z,
                            CONF_HEADING))


def to_code(config):
    rhs = App.make_hmc5883l(config.get(CONF_UPDATE_INTERVAL))
    hmc = Pvariable(config[CONF_ID], rhs)
    if CONF_ADDRESS in config:
        add(hmc.set_address(config[CONF_ADDRESS]))
    if CONF_RANGE in config:
        add(hmc.set_range(HMC5883L_RANGES[config[CONF_RANGE]]))
    if CONF_FIELD_STRENGTH_X in config:
        conf = config[CONF_FIELD_STRENGTH_X]
        sensor.register_sensor(hmc.Pmake_x_sensor(conf[CONF_NAME]), conf)
    if CONF_FIELD_STRENGTH_Y in config:
        conf = config[CONF_FIELD_STRENGTH_Y]
        sensor.register_sensor(hmc.Pmake_y_sensor(conf[CONF_NAME]), conf)
    if CONF_FIELD_STRENGTH_Z in config:
        conf = config[CONF_FIELD_STRENGTH_Z]
        sensor.register_sensor(hmc.Pmake_z_sensor(conf[CONF_NAME]), conf)
    if CONF_HEADING in config:
        conf = config[CONF_HEADING]
        sensor.register_sensor(hmc.Pmake_heading_sensor(conf[CONF_NAME]), conf)


BUILD_FLAGS = '-DUSE_HMC5883L'
