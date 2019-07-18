# coding=utf-8
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (CONF_ADDRESS, CONF_ID, CONF_SAMPLING, CONF_DATA_RATE, CONF_MEASUREMENT_MODE,
                           CONF_RANGE, ICON_MAGNET, UNIT_MICROTESLA, UNIT_DEGREES, ICON_SCREEN_ROTATION)

DEPENDENCIES = ['i2c']

hmc5883l_ns = cg.esphome_ns.namespace('hmc5883l')

CONF_FIELD_STRENGTH_X = 'field_strength_x'
CONF_FIELD_STRENGTH_Y = 'field_strength_y'
CONF_FIELD_STRENGTH_Z = 'field_strength_z'
CONF_HEADING = 'heading'

HMC5883LComponent = hmc5883l_ns.class_('HMC5883LComponent', cg.PollingComponent, i2c.I2CDevice)

HMC5883LSampling = hmc5883l_ns.enum('HMC5883LSampling')
HMC5883LSamplings = {
    1: HMC5883LSampling.HMC5883L_SAMPLING_1,
    2: HMC5883LSampling.HMC5883L_SAMPLING_2,
    4: HMC5883LSampling.HMC5883L_SAMPLING_4,
    8: HMC5883LSampling.HMC5883L_SAMPLING_8,
}

HMC5883LDatarate = hmc5883l_ns.enum('HMC5883LDatarate')
HMC5883LDatarates = {
    '0.75': HMC5883LDatarate.HMC5883L_DATARATE_0_75_HZ,
    '1.5': HMC5883LDatarate.HMC5883L_DATARATE_1_5_HZ,
    '3.0': HMC5883LDatarate.HMC5883L_DATARATE_3_0_HZ,
    '7.5': HMC5883LDatarate.HMC5883L_DATARATE_7_5_HZ,
    '15': HMC5883LDatarate.HMC5883L_DATARATE_15_0_HZ,
    '30': HMC5883LDatarate.HMC5883L_DATARATE_30_0_HZ,
    '75': HMC5883LDatarate.HMC5883L_DATARATE_75_0_HZ,
}

HMC5883LMeasurementMode = hmc5883l_ns.enum('HMC5883LMeasurementMode')
HMC5883LMeasurementModes = {
    'normal': HMC5883LMeasurementMode.HMC5883L_MEASUREMENT_MODE_NORMAL,
    'pos_bias': HMC5883LMeasurementMode.HMC5883L_MEASUREMENT_MODE_POS_BIAS,
    'neg_bias': HMC5883LMeasurementMode.HMC5883L_MEASUREMENT_MODE_NEG_BIAS,
}

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
    cv.GenerateID(): cv.declare_id(HMC5883LComponent),
    cv.Optional(CONF_ADDRESS): cv.i2c_address,
    cv.Optional(CONF_SAMPLING, default='1'): validate_enum(HMC5883LSamplings),
    cv.Optional(CONF_DATA_RATE, default='15Hz'): validate_enum(
        HMC5883LDatarates, unit="Hz", int=False),
    cv.Optional(CONF_MEASUREMENT_MODE, default='normal'): validate_enum(
        HMC5883LMeasurementModes, int=False),
    cv.Optional(CONF_RANGE, default='130uT'): validate_enum(HMC5883L_RANGES, unit="uT"),
    cv.Optional(CONF_FIELD_STRENGTH_X): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Y): field_strength_schema,
    cv.Optional(CONF_FIELD_STRENGTH_Z): field_strength_schema,
    cv.Optional(CONF_HEADING): heading_schema,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x1E))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    cg.add(var.set_sampling(config[CONF_SAMPLING]))
    cg.add(var.set_datarate(config[CONF_DATA_RATE]))
    cg.add(var.set_measurement_mode(config[CONF_MEASUREMENT_MODE]))
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
