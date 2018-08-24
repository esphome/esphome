import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable

DEPENDENCIES = ['i2c']

CONF_ACCEL_X = 'accel_x'
CONF_ACCEL_Y = 'accel_y'
CONF_ACCEL_Z = 'accel_z'
CONF_GYRO_X = 'gyro_x'
CONF_GYRO_Y = 'gyro_y'
CONF_GYRO_Z = 'gyro_z'

MPU6050Component = sensor.sensor_ns.MPU6050Component
MPU6050AccelSensor = sensor.sensor_ns.MPU6050AccelSensor
MPU6050GyroSensor = sensor.sensor_ns.MPU6050GyroSensor
MPU6050TemperatureSensor = sensor.sensor_ns.MPU6050TemperatureSensor

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MPU6050Component),
    vol.Optional(CONF_ADDRESS, default=0x68): cv.i2c_address,
    vol.Optional(CONF_ACCEL_X): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_ACCEL_Y): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_ACCEL_Z): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_GYRO_X): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_GYRO_Y): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_GYRO_Z): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}), cv.has_at_least_one_key(CONF_ACCEL_X, CONF_ACCEL_Y, CONF_ACCEL_Z,
                            CONF_GYRO_X, CONF_GYRO_Y, CONF_GYRO_Z))


def to_code(config):
    rhs = App.make_mpu6050_sensor(config[CONF_ADDRESS], config.get(CONF_UPDATE_INTERVAL))
    mpu = Pvariable(config[CONF_ID], rhs)
    if CONF_ACCEL_X in config:
        conf = config[CONF_ACCEL_X]
        rhs = mpu.Pmake_accel_x_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_ACCEL_Y in config:
        conf = config[CONF_ACCEL_Y]
        rhs = mpu.Pmake_accel_y_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_ACCEL_Z in config:
        conf = config[CONF_ACCEL_Z]
        rhs = mpu.Pmake_accel_z_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_GYRO_X in config:
        conf = config[CONF_GYRO_X]
        rhs = mpu.Pmake_gyro_x_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_GYRO_Y in config:
        conf = config[CONF_GYRO_Y]
        rhs = mpu.Pmake_gyro_y_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_GYRO_Z in config:
        conf = config[CONF_GYRO_Z]
        rhs = mpu.Pmake_gyro_z_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)
    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        rhs = mpu.Pmake_temperature_sensor(conf[CONF_NAME])
        sensor.register_sensor(rhs, conf)


BUILD_FLAGS = '-DUSE_MPU6050'
