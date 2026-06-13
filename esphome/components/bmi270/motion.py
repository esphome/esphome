import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import (
    CONF_ACCELEROMETER_ODR,
    CONF_ACCELEROMETER_RANGE,
    CONF_GYROSCOPE_ODR,
    CONF_GYROSCOPE_RANGE,
)
from esphome.components.motion import motion_schema, new_motion_component
import esphome.config_validation as cv

from . import BMI270Component, bmi270_ns

DEPENDENCIES = ["i2c"]

#  Enum proxies (must match the C++ enum values exactly)
BMI270AccelRange = bmi270_ns.enum("BMI270AccelRange")
ACCEL_RANGE_OPTIONS = {
    "2G": BMI270AccelRange.BMI270_ACCEL_RANGE_2G,
    "4G": BMI270AccelRange.BMI270_ACCEL_RANGE_4G,
    "8G": BMI270AccelRange.BMI270_ACCEL_RANGE_8G,
    "16G": BMI270AccelRange.BMI270_ACCEL_RANGE_16G,
}

BMI270GyroRange = bmi270_ns.enum("BMI270GyroRange")
GYRO_RANGE_OPTIONS = {
    "2000DPS": BMI270GyroRange.BMI270_GYRO_RANGE_2000,
    "1000DPS": BMI270GyroRange.BMI270_GYRO_RANGE_1000,
    "500DPS": BMI270GyroRange.BMI270_GYRO_RANGE_500,
    "250DPS": BMI270GyroRange.BMI270_GYRO_RANGE_250,
    "125DPS": BMI270GyroRange.BMI270_GYRO_RANGE_125,
}

BMI270AccelODR = bmi270_ns.enum("BMI270AccelODR")
ACCEL_ODR_OPTIONS = {
    "12_5HZ": BMI270AccelODR.BMI270_ACCEL_ODR_12_5,
    "25HZ": BMI270AccelODR.BMI270_ACCEL_ODR_25,
    "50HZ": BMI270AccelODR.BMI270_ACCEL_ODR_50,
    "100HZ": BMI270AccelODR.BMI270_ACCEL_ODR_100,
    "200HZ": BMI270AccelODR.BMI270_ACCEL_ODR_200,
    "400HZ": BMI270AccelODR.BMI270_ACCEL_ODR_400,
    "800HZ": BMI270AccelODR.BMI270_ACCEL_ODR_800,
    "1600HZ": BMI270AccelODR.BMI270_ACCEL_ODR_1600,
}

BMI270GyroODR = bmi270_ns.enum("BMI270GyroODR")
GYRO_ODR_OPTIONS = {
    "25HZ": BMI270GyroODR.BMI270_GYRO_ODR_25,
    "50HZ": BMI270GyroODR.BMI270_GYRO_ODR_50,
    "100HZ": BMI270GyroODR.BMI270_GYRO_ODR_100,
    "200HZ": BMI270GyroODR.BMI270_GYRO_ODR_200,
    "400HZ": BMI270GyroODR.BMI270_GYRO_ODR_400,
    "800HZ": BMI270GyroODR.BMI270_GYRO_ODR_800,
    "1600HZ": BMI270GyroODR.BMI270_GYRO_ODR_1600,
    "3200HZ": BMI270GyroODR.BMI270_GYRO_ODR_3200,
}

#  Top-level CONFIG_SCHEMA
CONFIG_SCHEMA = (
    motion_schema(BMI270Component, has_accel=True, has_gyro=True)
    .extend(
        {
            cv.Optional(CONF_ACCELEROMETER_RANGE, default="4G"): cv.enum(
                ACCEL_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ACCELEROMETER_ODR, default="100HZ"): cv.enum(
                ACCEL_ODR_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_RANGE, default="2000DPS"): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_ODR, default="200HZ"): cv.enum(
                GYRO_ODR_OPTIONS, upper=True
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x68))
)


#  Code generation
async def to_code(config):
    var = await new_motion_component(config)
    await i2c.register_i2c_device(var, config)

    # Accelerometer sensors
    # Hardware configuration
    cg.add(var.set_accel_range(config[CONF_ACCELEROMETER_RANGE]))
    cg.add(var.set_accel_odr(config[CONF_ACCELEROMETER_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYROSCOPE_RANGE]))
    cg.add(var.set_gyro_odr(config[CONF_GYROSCOPE_ODR]))
