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
from esphome.types import ConfigType

from . import BMI270Component, bmi270_ns

DEPENDENCIES = ["i2c"]

CONF_AUX_ADDRESS = "aux_address"
CONF_AUX_DEVICE = "aux_device"

# to_code skips the setters at these values, so each must match its member default in bmi270.h
DEFAULT_ACCELEROMETER_RANGE = "4G"
DEFAULT_ACCELEROMETER_ODR = "100HZ"
DEFAULT_GYROSCOPE_RANGE = "2000DPS"
DEFAULT_GYROSCOPE_ODR = "200HZ"
# The BMM150's fixed address
DEFAULT_AUX_ADDRESS = 0x10

#  Enum proxies (must match the C++ enum values exactly)
BMI270AuxDevice = bmi270_ns.enum("BMI270AuxDevice")
BMI270AccelRange = bmi270_ns.enum("BMI270AccelRange")
ACCEL_RANGE_OPTIONS = BMI270AccelRange.options(
    "2G", "4G", "8G", "16G", prefix="BMI270_ACCEL_RANGE_"
)

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

CONFIG_SCHEMA = (
    motion_schema(BMI270Component, has_accel=True, has_gyro=True)
    .extend(
        {
            cv.Optional(
                CONF_ACCELEROMETER_RANGE, default=DEFAULT_ACCELEROMETER_RANGE
            ): cv.enum(ACCEL_RANGE_OPTIONS, upper=True),
            cv.Optional(
                CONF_ACCELEROMETER_ODR, default=DEFAULT_ACCELEROMETER_ODR
            ): cv.enum(ACCEL_ODR_OPTIONS, upper=True),
            cv.Optional(CONF_GYROSCOPE_RANGE, default=DEFAULT_GYROSCOPE_RANGE): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_ODR, default=DEFAULT_GYROSCOPE_ODR): cv.enum(
                GYRO_ODR_OPTIONS, upper=True
            ),
            cv.Optional(CONF_AUX_DEVICE, default="NONE"): cv.one_of(
                "NONE", "BMM150", upper=True
            ),
            cv.Optional(CONF_AUX_ADDRESS, default=DEFAULT_AUX_ADDRESS): cv.i2c_address,
        }
    )
    .extend(i2c.i2c_device_schema(0x68))
)


async def to_code(config: ConfigType) -> None:
    var = await new_motion_component(config)
    await i2c.register_i2c_device(var, config)

    # The C++ members already hold the schema defaults
    for key, default, setter in (
        (CONF_ACCELEROMETER_RANGE, DEFAULT_ACCELEROMETER_RANGE, var.set_accel_range),
        (CONF_ACCELEROMETER_ODR, DEFAULT_ACCELEROMETER_ODR, var.set_accel_odr),
        (CONF_GYROSCOPE_RANGE, DEFAULT_GYROSCOPE_RANGE, var.set_gyro_range),
        (CONF_GYROSCOPE_ODR, DEFAULT_GYROSCOPE_ODR, var.set_gyro_odr),
    ):
        if config[key] != default:
            cg.add(setter(config[key]))
    # The aux interface code only compiles when some instance has a BMM150; the
    # instance still records its own device, as another one may have none
    if config[CONF_AUX_DEVICE] == "BMM150":
        cg.add_define("USE_BMI270_BMM150")
        cg.add(var.set_aux_device(BMI270AuxDevice.BMI270_AUX_DEVICE_BMM150))
        if (address := config[CONF_AUX_ADDRESS]) != DEFAULT_AUX_ADDRESS:
            cg.add(var.set_aux_device_address(address))
