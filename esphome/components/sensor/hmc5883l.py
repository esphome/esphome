# coding=utf-8
import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_RANGE, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'
CONF_HEADING = 'heading'

HMC5883LComponent = sensor.sensor_ns.class_('HMC5883LComponent', PollingComponent, i2c.I2CDevice)
HMC5883LFieldStrengthSensor = sensor.sensor_ns.class_('HMC5883LFieldStrengthSensor',
                                                      sensor.EmptyPollingParentSensor)
HMC5883LHeadingSensor = sensor.sensor_ns.class_('HMC5883LHeadingSensor',
                                                sensor.EmptyPollingParentSensor)

HMC5883LRange = sensor.sensor_ns.enum('HMC5883LRange')
HMC5883L_RANGES = {
    88: HMC5883LRange.HMC5883L_RANGE_88_UT,
    130: HMC5883LRange.HMC5883L_RANGE_130_UT,
    190: HMC5883LRange.HMC5883L_RANGE_190_UT,
    250: HMC5883LRange.HMC5883L_RANGE_250_UT,
    400: HMC5883LRange.HMC5883L_RANGE_400_UT,
    470: HMC5883LRange.HMC5883L_RANGE_470_UT,
    560: HMC5883LRange.HMC5883L_RANGE_560_UT,
    810: HMC5883LRange.HMC5883L_RANGE_810_UT,
}


def validate_range(value):
    value = cv.string(value)
    if value.endswith(u'µT') or value.endswith('uT'):
        value = value[:-2]
    return cv.one_of(*HMC5883L_RANGES, int=True)(value)


SENSOR_KEYS = [CONF_FIELD_STRENGTH_X, CONF_FIELD_STRENGTH_Y, CONF_FIELD_STRENGTH_Z,
               CONF_HEADING]

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(HMC5883LComponent),
    vol.Optional(CONF_ADDRESS): cv.i2c_address,
    vol.Optional(CONF_FIELD_STRENGTH_X): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HMC5883LFieldStrengthSensor),
    })),
    vol.Optional(CONF_FIELD_STRENGTH_Y): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HMC5883LFieldStrengthSensor),
    })),
    vol.Optional(CONF_FIELD_STRENGTH_Z): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HMC5883LFieldStrengthSensor),
    })),
    vol.Optional(CONF_HEADING): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(HMC5883LHeadingSensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
    vol.Optional(CONF_RANGE): validate_range,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSOR_KEYS))


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
    setup_component(hmc, config)


BUILD_FLAGS = '-DUSE_HMC5883L'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_FIELD_STRENGTH_X, CONF_FIELD_STRENGTH_Y, CONF_FIELD_STRENGTH_Z, CONF_HEADING):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
