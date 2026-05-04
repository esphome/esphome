import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.motion import (
    MotionComponent,
    motion_schema,
    new_motion_component,
)
import esphome.config_validation as cv

# ── Dependency declarations ──────────────────────────────────────────────────
DEPENDENCIES = ["i2c", "motion"]
DOMAIN = "lsm6ds3"

# ── C++ namespace / class ────────────────────────────────────────────────────
lsm6ds3trc_ns = cg.esphome_ns.namespace("lsm6ds3trc")
LSM6DS3TRCComponent = lsm6ds3trc_ns.class_(
    "LSM6DS3TRCComponent",
    MotionComponent,
    i2c.I2CDevice,
)

# ── Enum proxies ─────────────────────────────────────────────────────────────
LSM6DS3TRCAccelRange = lsm6ds3trc_ns.enum("LSM6DS3TRCAccelRange")
ACCEL_RANGE_OPTIONS = {
    "2G": LSM6DS3TRCAccelRange.LSM6DS3TRC_ACCEL_RANGE_2G,
    "4G": LSM6DS3TRCAccelRange.LSM6DS3TRC_ACCEL_RANGE_4G,
    "8G": LSM6DS3TRCAccelRange.LSM6DS3TRC_ACCEL_RANGE_8G,
    "16G": LSM6DS3TRCAccelRange.LSM6DS3TRC_ACCEL_RANGE_16G,
}

LSM6DS3TRCAccelODR = lsm6ds3trc_ns.enum("LSM6DS3TRCAccelODR")
ACCEL_ODR_OPTIONS = {
    "1_6HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_1_6,
    "12_5HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_12_5,
    "26HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_26,
    "52HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_52,
    "104HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_104,
    "208HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_208,
    "416HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_416,
    "833HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_833,
    "1666HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_1666,
    "3332HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_3332,
    "6664HZ": LSM6DS3TRCAccelODR.LSM6DS3TRC_ACCEL_ODR_6664,
}

LSM6DS3TRCGyroRange = lsm6ds3trc_ns.enum("LSM6DS3TRCGyroRange")
GYRO_RANGE_OPTIONS = {
    "125DPS": LSM6DS3TRCGyroRange.LSM6DS3TRC_GYRO_RANGE_125,
    "250DPS": LSM6DS3TRCGyroRange.LSM6DS3TRC_GYRO_RANGE_250,
    "500DPS": LSM6DS3TRCGyroRange.LSM6DS3TRC_GYRO_RANGE_500,
    "1000DPS": LSM6DS3TRCGyroRange.LSM6DS3TRC_GYRO_RANGE_1000,
    "2000DPS": LSM6DS3TRCGyroRange.LSM6DS3TRC_GYRO_RANGE_2000,
}

LSM6DS3TRCGyroODR = lsm6ds3trc_ns.enum("LSM6DS3TRCGyroODR")
GYRO_ODR_OPTIONS = {
    "12_5HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_12_5,
    "26HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_26,
    "52HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_52,
    "104HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_104,
    "208HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_208,
    "416HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_416,
    "833HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_833,
    "1666HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_1666,
    "3332HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_3332,
    "6664HZ": LSM6DS3TRCGyroODR.LSM6DS3TRC_GYRO_ODR_6664,
}

# ── CONFIG_SCHEMA ─────────────────────────────────────────────────────────────
# Extend the motion platform schema which provides:
#   - accel_x/y/z sensor schemas
#   - gyro_x/y/z sensor schemas
#   - temperature sensor schema (wired as a lazy callback in C++)
#   - axis_mapping schema + validation
#   - update_interval / polling
CONFIG_SCHEMA = (
    motion_schema(LSM6DS3TRCComponent, has_accel=True, has_gyro=True)
    .extend(
        {
            cv.Optional(CONF_ACCELERO_RANGE, default="4G"): cv.enum(
                ACCEL_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ACCEL_ODR, default="104HZ"): cv.enum(
                ACCEL_ODR_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYRO_RANGE, default="2000DPS"): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYRO_ODR, default="208HZ"): cv.enum(
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
    cg.add(var.set_accel_range(config[CONF_ACCEL_RANGE]))
    cg.add(var.set_accel_odr(config[CONF_ACCEL_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYRO_RANGE]))
    cg.add(var.set_gyro_odr(config[CONF_GYRO_ODR]))
