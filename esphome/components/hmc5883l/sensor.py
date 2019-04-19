# coding=utf-8
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_RANGE, ICON_MAGNET, UNIT_MICROTESLA, \
    UNIT_DEGREES, ICON_SCREEN_ROTATION

DEPENDENCIES = ['i2c']

hmc5883l_ns = cg.esphome_ns.namespace('hmc5883l')

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'
CONF_HEADING = 'heading'

HMC5883LComponent = hmc5883l_ns.class_('HMC5883LComponent', cg.PollingComponent, i2c.I2CDevice)

HMC5883LRange = hmc5883l_ns.enum('HMC5883LRange')
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
    return cv.enum(HMC5883L_RANGES, int=True)(value)


field_strength_schema = sensor.sensor_schema(UNIT_MICROTESLA, ICON_MAGNET, 1)
heading_schema = sensor.sensor_schema(UNIT_DEGREES, ICON_SCREEN_ROTATION, 1)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HMC5883LComponent),
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
    cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
    cv.Optional(CONF_HEADING): heading_schema,
    cv.Optional(CONF_RANGE, default='130uT'): validate_range,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x1E))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_range(config[CONF_RANGE]))
    if CONF_FIELD_STRENGTH_X in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_X])
        cg.add(var.set_x_sensor(sens))
    if CONF_FIELD_STRENGTH_Y in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_Y])
        cg.add(var.set_y_sensor(sens))
    if CONF_FIELD_STRENGTH_Z in config:
        sens = yield sensor.new_sensor(config[CONF_FIELD_STRENGTH_Z])
        cg.add(var.set_z_sensor(sens))
    if CONF_HEADING in config:
        sens = yield sensor.new_sensor(config[CONF_HEADING])
        cg.add(var.set_heading_sensor(sens))
