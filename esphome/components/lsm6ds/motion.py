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

from . import LSM6DSComponent, lsm6ds_ns

# ── Dependency declarations ──────────────────────────────────────────────────
DEPENDENCIES = ["i2c"]
DOMAIN = "lsm6ds"

# ── C++ namespace / class ────────────────────────────────────────────────────
# ── Enum proxies ─────────────────────────────────────────────────────────────
LSM6DSAccelRange = lsm6ds_ns.enum("LSM6DSAccelRange")
ACCEL_RANGE_OPTIONS = {
    "2G": LSM6DSAccelRange.LSM6DS_ACCEL_RANGE_2G,
    "4G": LSM6DSAccelRange.LSM6DS_ACCEL_RANGE_4G,
    "8G": LSM6DSAccelRange.LSM6DS_ACCEL_RANGE_8G,
    "16G": LSM6DSAccelRange.LSM6DS_ACCEL_RANGE_16G,
}

LSM6DSAccelODR = lsm6ds_ns.enum("LSM6DSAccelODR")
ACCEL_ODR_OPTIONS = {
    "OFF": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_OFF,
    "12_5HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_12_5,
    "26HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_26,
    "52HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_52,
    "104HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_104,
    "208HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_208,
    "416HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_416,
    "833HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_833,
    "1666HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_1666,
    "3332HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_3332,
    "6664HZ": LSM6DSAccelODR.LSM6DS_ACCEL_ODR_6664,
}

LSM6DSGyroRange = lsm6ds_ns.enum("LSM6DSGyroRange")
GYRO_RANGE_OPTIONS = {
    "125DPS": LSM6DSGyroRange.LSM6DS_GYRO_RANGE_125,
    "250DPS": LSM6DSGyroRange.LSM6DS_GYRO_RANGE_250,
    "500DPS": LSM6DSGyroRange.LSM6DS_GYRO_RANGE_500,
    "1000DPS": LSM6DSGyroRange.LSM6DS_GYRO_RANGE_1000,
    "2000DPS": LSM6DSGyroRange.LSM6DS_GYRO_RANGE_2000,
}

LSM6DSGyroODR = lsm6ds_ns.enum("LSM6DSGyroODR")
GYRO_ODR_OPTIONS = {
    "OFF": LSM6DSGyroODR.LSM6DS_GYRO_ODR_OFF,
    "12_5HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_12_5,
    "26HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_26,
    "52HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_52,
    "104HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_104,
    "208HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_208,
    "416HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_416,
    "833HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_833,
    "1666HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_1666,
    "3332HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_3332,
    "6664HZ": LSM6DSGyroODR.LSM6DS_GYRO_ODR_6664,
}

# ── CONFIG_SCHEMA ─────────────────────────────────────────────────────────────
# Extend the motion platform schema which provides:
#   - accel_x/y/z sensor schemas
#   - gyro_x/y/z sensor schemas
#   - axis_mapping schema + validation
#   - update_interval / polling
CONFIG_SCHEMA = (
    motion_schema(LSM6DSComponent, has_accel=True, has_gyro=True)
    .extend(
        {
            cv.Optional(CONF_ACCELEROMETER_RANGE, default="4G"): cv.enum(
                ACCEL_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ACCELEROMETER_ODR, default="104HZ"): cv.enum(
                ACCEL_ODR_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_RANGE, default="2000DPS"): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYROSCOPE_ODR, default="208HZ"): cv.enum(
                GYRO_ODR_OPTIONS, upper=True
            ),
        }
    )
    .extend(i2c.i2c_device_schema(0x6A))
)


# ── Code generation ──────────────────────────────────────────────────────────
async def to_code(config):
    var = await new_motion_component(config)

    # Let the motion platform handle sensor wiring, axis mapping, and polling
    await i2c.register_i2c_device(var, config)

    # Chip-specific hardware configuration
    cg.add(var.set_accel_range(config[CONF_ACCELEROMETER_RANGE]))
    cg.add(var.set_accel_odr(config[CONF_ACCELEROMETER_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYROSCOPE_RANGE]))
    cg.add(var.set_gyro_odr(config[CONF_GYROSCOPE_ODR]))
