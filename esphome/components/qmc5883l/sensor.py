# coding=utf-8
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (CONF_ADDRESS, CONF_ID, CONF_OVERSAMPLING, CONF_DATA_RATE, CONF_RANGE,
                           ICON_MAGNET, UNIT_MICROTESLA, UNIT_DEGREES, ICON_SCREEN_ROTATION)

DEPENDENCIES = ['i2c']

qmc5883l_ns = cg.esphome_ns.namespace('qmc5883l')

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'
CONF_HEADING = 'heading'

QMC5883LComponent = qmc5883l_ns.class_('QMC5883LComponent', cg.PollingComponent, i2c.I2CDevice)

QMC5883LDatarate = qmc5883l_ns.enum('QMC5883LDatarate')
QMC5883LDatarates = {
    10: QMC5883LDatarate.QMC5883L_DATARATE_10_HZ,
    50: QMC5883LDatarate.QMC5883L_DATARATE_50_HZ,
    100: QMC5883LDatarate.QMC5883L_DATARATE_100_HZ,
    200: QMC5883LDatarate.QMC5883L_DATARATE_200_HZ,
}

QMC5883LRange = qmc5883l_ns.enum('QMC5883LRange')
QMC5883L_RANGES = {
    200: QMC5883LRange.QMC5883L_RANGE_200_UT,
    800: QMC5883LRange.QMC5883L_RANGE_800_UT,
}

QMC5883LOversampling = qmc5883l_ns.enum('QMC5883LOversampling')
QMC5883LOversamplings = {
    512: QMC5883LOversampling.QMC5883L_SAMPLING_512,
    256: QMC5883LOversampling.QMC5883L_SAMPLING_256,
    128: QMC5883LOversampling.QMC5883L_SAMPLING_128,
    64: QMC5883LOversampling.QMC5883L_SAMPLING_64,
}


def validate_enum(enum_values, unit=None, int=True):
    def validate_enum_bound(value):
        value = cv.string(value)
        if unit and (value.endswith(unit.encode(encoding='UTF-8', errors='strict'))
                     or value.endswith(unit)):
            value = value[:-len(unit)]
        return cv.enum(enum_values, int=int)(value)
    return validate_enum_bound


field_strength_schema = sensor.sensor_schema(UNIT_MICROTESLA, ICON_MAGNET, 1)
heading_schema = sensor.sensor_schema(UNIT_DEGREES, ICON_SCREEN_ROTATION, 1)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(QMC5883LComponent),
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
    cv.Optional(CONF_DATA_RATE, default='10Hz'): validate_enum(QMC5883LDatarates, unit="Hz"),
    cv.Optional(CONF_RANGE, default='200uT'): validate_enum(QMC5883L_RANGES, unit="uT"),
    cv.Optional(CONF_OVERSAMPLING, default=512): validate_enum(QMC5883LOversamplings),
    cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
    cv.Optional(CONF_HEADING): heading_schema,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x0D))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_oversampling(config[CONF_OVERSAMPLING]))
    cg.add(var.set_datarate(config[CONF_DATA_RATE]))
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
