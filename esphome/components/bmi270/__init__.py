import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION,
    ICON_ROTATE_RIGHT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_METER_PER_SECOND_SQUARED,
)

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

# ── YAML config keys ─────────────────────────────────────────────────────────
CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"
CONF_GYRO_X = "gyro_x"
CONF_GYRO_Y = "gyro_y"
CONF_GYRO_Z = "gyro_z"
CONF_ACCEL_RANGE = "accel_range"
CONF_ACCEL_ODR = "accel_odr"
CONF_GYRO_RANGE = "gyro_range"
CONF_GYRO_ODR = "gyro_odr"


# ── Sensor schema helpers ────────────────────────────────────────────────────
def _accel_sensor_schema(axis: str):
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
        icon=ICON_ACCELERATION,
        accuracy_decimals=4,
        state_class=STATE_CLASS_MEASUREMENT,
    )


def _gyro_sensor_schema(axis: str):
    return sensor.sensor_schema(
        unit_of_measurement="°/s",
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=4,
        state_class=STATE_CLASS_MEASUREMENT,
    )


# ── Top-level CONFIG_SCHEMA ──────────────────────────────────────────────────
CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BMI270Component),
            # Accelerometer axes
            cv.Optional(CONF_ACCEL_X): _accel_sensor_schema("x"),
            cv.Optional(CONF_ACCEL_Y): _accel_sensor_schema("y"),
            cv.Optional(CONF_ACCEL_Z): _accel_sensor_schema("z"),
            # Gyroscope axes
            cv.Optional(CONF_GYRO_X): _gyro_sensor_schema("x"),
            cv.Optional(CONF_GYRO_Y): _gyro_sensor_schema("y"),
            cv.Optional(CONF_GYRO_Z): _gyro_sensor_schema("z"),
            # Temperature
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                state_class=STATE_CLASS_MEASUREMENT,
                device_class=DEVICE_CLASS_TEMPERATURE,
            ),
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
    for conf_key, setter in [
        (CONF_ACCEL_X, "set_accel_x_sensor"),
        (CONF_ACCEL_Y, "set_accel_y_sensor"),
        (CONF_ACCEL_Z, "set_accel_z_sensor"),
    ]:
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(getattr(var, setter)(sens))

    # Gyroscope sensors
    for conf_key, setter in [
        (CONF_GYRO_X, "set_gyro_x_sensor"),
        (CONF_GYRO_Y, "set_gyro_y_sensor"),
        (CONF_GYRO_Z, "set_gyro_z_sensor"),
    ]:
        if conf_key in config:
            sens = await sensor.new_sensor(config[conf_key])
            cg.add(getattr(var, setter)(sens))

    # Temperature sensor
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))

    # Hardware configuration
    cg.add(var.set_accel_range(config[CONF_ACCEL_RANGE]))
    cg.add(var.set_accel_odr(config[CONF_ACCEL_ODR]))
    cg.add(var.set_gyro_range(config[CONF_GYRO_RANGE]))
    cg.add(var.set_gyro_odr(config[CONF_GYRO_ODR]))
