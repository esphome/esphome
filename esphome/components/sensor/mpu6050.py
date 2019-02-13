import voluptuous as vol

from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_ID, CONF_NAME, CONF_TEMPERATURE, \
    CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent

DEPENDENCIES = ['i2c']

CONF_ACCEL_X = 'accel_x'
CONF_ACCEL_Y = 'accel_y'
CONF_ACCEL_Z = 'accel_z'
CONF_GYRO_X = 'gyro_x'
CONF_GYRO_Y = 'gyro_y'
CONF_GYRO_Z = 'gyro_z'

MPU6050Component = sensor.sensor_ns.class_('MPU6050Component', PollingComponent, i2c.I2CDevice)
MPU6050AccelSensor = sensor.sensor_ns.class_('MPU6050AccelSensor', sensor.EmptyPollingParentSensor)
MPU6050GyroSensor = sensor.sensor_ns.class_('MPU6050GyroSensor', sensor.EmptyPollingParentSensor)
MPU6050TemperatureSensor = sensor.sensor_ns.class_('MPU6050TemperatureSensor',
                                                   sensor.EmptyPollingParentSensor)

SENSOR_KEYS = [CONF_ACCEL_X, CONF_ACCEL_Y, CONF_ACCEL_Z,
               CONF_GYRO_X, CONF_GYRO_Y, CONF_GYRO_Z]

PLATFORM_SCHEMA = vol.All(sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(MPU6050Component),
    vol.Optional(CONF_ADDRESS, default=0x68): cv.i2c_address,
    vol.Optional(CONF_ACCEL_X): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050AccelSensor),
    })),
    vol.Optional(CONF_ACCEL_Y): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050AccelSensor),
    })),
    vol.Optional(CONF_ACCEL_Z): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050AccelSensor),
    })),
    vol.Optional(CONF_GYRO_X): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050GyroSensor),
    })),
    vol.Optional(CONF_GYRO_Y): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050GyroSensor),
    })),
    vol.Optional(CONF_GYRO_Z): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050GyroSensor),
    })),
    vol.Optional(CONF_TEMPERATURE): cv.nameable(sensor.SENSOR_SCHEMA.extend({
        cv.GenerateID(): cv.declare_variable_id(MPU6050TemperatureSensor),
    })),
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema), cv.has_at_least_one_key(*SENSOR_KEYS))


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

    setup_component(mpu, config)


BUILD_FLAGS = '-DUSE_MPU6050'


def to_hass_config(data, config):
    ret = []
    for key in (CONF_ACCEL_X, CONF_ACCEL_Y, CONF_ACCEL_Z, CONF_GYRO_X, CONF_GYRO_Y, CONF_GYRO_Z,
                CONF_TEMPERATURE):
        if key in config:
            ret.append(sensor.core_to_hass_config(data, config[key]))
    return ret
