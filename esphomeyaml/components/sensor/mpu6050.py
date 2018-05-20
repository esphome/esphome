import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_ADDRESS, CONF_MAKE_ID, CONF_MQTT_ID, CONF_NAME, \
    CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Pvariable

DEPENDENCIES = ['i2c']

CONF_ACCEL_X = 'accel_x'
CONF_ACCEL_Y = 'accel_y'
CONF_ACCEL_Z = 'accel_z'
CONF_GYRO_X = 'gyro_x'
CONF_GYRO_Y = 'gyro_y'
CONF_GYRO_Z = 'gyro_z'

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('mpu6050', CONF_MAKE_ID): cv.register_variable_id,
    vol.Optional(CONF_ADDRESS, default=0x68): cv.i2c_address,
    vol.Optional(CONF_ACCEL_X): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_ACCEL_Y): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_ACCEL_Z): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_GYRO_X): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_GYRO_Y): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_GYRO_Z): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_TEMPERATURE): sensor.SENSOR_SCHEMA,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}), cv.has_at_least_one_key(CONF_ACCEL_X, CONF_ACCEL_Y, CONF_ACCEL_Z,
                            CONF_GYRO_X, CONF_GYRO_Y, CONF_GYRO_Z))

MPU6050Component = sensor.sensor_ns.MPU6050Component
MPU6050AccelSensor = sensor.sensor_ns.MPU6050AccelSensor
MPU6050GyroSensor = sensor.sensor_ns.MPU6050GyroSensor
MPU6050TemperatureSensor = sensor.sensor_ns.MPU6050TemperatureSensor


def to_code(config):
    rhs = App.make_mpu6050_sensor(config[CONF_ADDRESS], config.get(CONF_UPDATE_INTERVAL))
    mpu = Pvariable(MPU6050Component, config[CONF_MAKE_ID], rhs)
    if CONF_ACCEL_X in config:
        conf = config[CONF_ACCEL_X]
        rhs = mpu.Pmake_accel_x_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050AccelSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_ACCEL_Y in config:
        conf = config[CONF_ACCEL_Y]
        rhs = mpu.Pmake_accel_y_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050AccelSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_ACCEL_Z in config:
        conf = config[CONF_ACCEL_Z]
        rhs = mpu.Pmake_accel_z_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050AccelSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_GYRO_X in config:
        conf = config[CONF_GYRO_X]
        rhs = mpu.Pmake_gyro_x_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050GyroSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_GYRO_Y in config:
        conf = config[CONF_GYRO_Y]
        rhs = mpu.Pmake_gyro_y_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050GyroSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_GYRO_Z in config:
        conf = config[CONF_GYRO_Z]
        rhs = mpu.Pmake_gyro_z_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050GyroSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)
    if CONF_TEMPERATURE in config:
        conf = config[CONF_TEMPERATURE]
        rhs = mpu.Pmake_temperature_sensor(conf[CONF_NAME])
        sensor_ = Pvariable(MPU6050TemperatureSensor, conf[CONF_MQTT_ID], rhs)
        sensor.register_sensor(sensor_, conf)


BUILD_FLAGS = '-DUSE_MPU6050'
