import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, CONF_TEMPERATURE, \
    ICON_BRIEFCASE_DOWNLOAD, UNIT_METER_PER_SECOND_SQUARED, \
    ICON_SCREEN_ROTATION, UNIT_DEGREE_PER_SECOND, ICON_THERMOMETER, UNIT_CELSIUS

DEPENDENCIES = ['i2c']

CONF_ACCEL_X = 'accel_x'
CONF_ACCEL_Y = 'accel_y'
CONF_ACCEL_Z = 'accel_z'
CONF_GYRO_X = 'gyro_x'
CONF_GYRO_Y = 'gyro_y'
CONF_GYRO_Z = 'gyro_z'

mpu6050_ns = cg.esphome_ns.namespace('mpu6050')
MPU6050Component = mpu6050_ns.class_('MPU6050Component', cg.PollingComponent, i2c.I2CDevice)

accel_schema = sensor.sensor_schema(UNIT_METER_PER_SECOND_SQUARED, ICON_BRIEFCASE_DOWNLOAD, 2)
gyro_schema = sensor.sensor_schema(UNIT_DEGREE_PER_SECOND, ICON_SCREEN_ROTATION, 2)
temperature_schema = sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(MPU6050Component),
    cv.Optional(CONF_ACCEL_X): accel_schema,
    cv.Optional(CONF_ACCEL_Y): accel_schema,
    cv.Optional(CONF_ACCEL_Z): accel_schema,
    cv.Optional(CONF_GYRO_X): gyro_schema,
    cv.Optional(CONF_GYRO_Y): gyro_schema,
    cv.Optional(CONF_GYRO_Z): gyro_schema,
    cv.Optional(CONF_TEMPERATURE): temperature_schema,
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x68))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    for d in ['x', 'y', 'z']:
        accel_key = f'accel_{d}'
        if accel_key in config:
            sens = yield sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f'set_accel_{d}_sensor')(sens))
        accel_key = f'gyro_{d}'
        if accel_key in config:
            sens = yield sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f'set_gyro_{d}_sensor')(sens))

    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
