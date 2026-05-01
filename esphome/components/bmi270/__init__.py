import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_ID

# ── Dependency declarations ──────────────────────────────────────────────────
DEPENDENCIES = ["i2c"]

# ── C++ namespace / class ────────────────────────────────────────────────────
bmi270_ns = cg.esphome_ns.namespace("bmi270")
BMI270Component = bmi270_ns.class_(
    "BMI270Component", cg.PollingComponent, i2c.I2CDevice
)

# ── Enum proxies (must match the C++ enum values exactly) ────────────────────
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

BMI270AccelData = bmi270_ns.class_("BMI270AccelData")

CONF_ACCEL_RANGE = "accel_range"
CONF_ACCEL_ODR = "accel_odr"
CONF_GYRO_RANGE = "gyro_range"
CONF_GYRO_ODR = "gyro_odr"
CONF_BMI270_ID = "bmi270_id"

SENSOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BMI270_ID): cv.use_id(BMI270Component),
    }
)
# ── Top-level CONFIG_SCHEMA ──────────────────────────────────────────────────
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMI270Component),
            # Accelerometer axes
            # Hardware configuration
            cv.Optional(CONF_ACCEL_RANGE, default="4G"): cv.enum(
                ACCEL_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ACCEL_ODR, default="100HZ"): cv.enum(
                ACCEL_ODR_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYRO_RANGE, default="2000DPS"): cv.enum(
                GYRO_RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_GYRO_ODR, default="200HZ"): cv.enum(
                GYRO_ODR_OPTIONS, upper=True
            ),
        }
    )
    .extend(cv.polling_component_schema("50ms"))
    .extend(i2c.i2c_device_schema(0x68))
)


# ── Code generation ──────────────────────────────────────────────────────────
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Accelerometer sensors
    # Hardware configuration
    cg.add(var.set_accel_range(config[CONF_ACCEL_RANGE]))
    cg.add(var.set_accel_odr(config[CONF_ACCEL_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYRO_RANGE]))
    cg.add(var.set_gyro_odr(config[CONF_GYRO_ODR]))
