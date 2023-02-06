import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_BRIEFCASE_DOWNLOAD,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER_PER_SECOND_SQUARED,
    ICON_SCREEN_ROTATION,
    UNIT_DEGREE_PER_SECOND,
    UNIT_CELSIUS,
)

DEPENDENCIES = ["i2c"]

CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"
CONF_GYRO_X = "gyro_x"
CONF_GYRO_Y = "gyro_y"
CONF_GYRO_Z = "gyro_z"
CONF_HIGH_PERF = "high_perf"
CONF_SAMPLE_GYRO_RATE = "sample_gyro_rate"
CONF_SAMPLE_ACCL_RATE = "sample_accl_rate"
CONF_POWER_SAVE = "power_save"

lsm6ds3_ns = cg.esphome_ns.namespace("LSM6DS3")
LSM6DS3Component = lsm6ds3_ns.class_(
    "LSM6DS3Component", cg.PollingComponent, i2c.I2CDevice
)

accel_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_METER_PER_SECOND_SQUARED,
    icon=ICON_BRIEFCASE_DOWNLOAD,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)
gyro_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREE_PER_SECOND,
    icon=ICON_SCREEN_ROTATION,
    accuracy_decimals=2,
    state_class=STATE_CLASS_MEASUREMENT,
)
temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

validate_sample_accl_rate = cv.one_of(
    13, 26, 52, 104, 208, 416, 833, 1660, 3330, 6660, 13330
)
validate_sample_gyro_rate = cv.one_of(13, 26, 52, 104, 208, 416, 833, 1660)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LSM6DS3Component),
            cv.Optional(CONF_ACCEL_X): accel_schema,
            cv.Optional(CONF_ACCEL_Y): accel_schema,
            cv.Optional(CONF_ACCEL_Z): accel_schema,
            cv.Optional(CONF_GYRO_X): gyro_schema,
            cv.Optional(CONF_GYRO_Y): gyro_schema,
            cv.Optional(CONF_GYRO_Z): gyro_schema,
            cv.Optional(CONF_TEMPERATURE): temperature_schema,
            cv.Optional(CONF_HIGH_PERF, default=False): cv.boolean,
            cv.Optional(CONF_SAMPLE_ACCL_RATE, default=13): validate_sample_accl_rate,
            cv.Optional(CONF_SAMPLE_GYRO_RATE, default=13): validate_sample_gyro_rate,
            cv.Optional(CONF_POWER_SAVE, default=True): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6A))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_high_perf(config[CONF_HIGH_PERF]))
    cg.add(var.set_sample_accl_rate(config[CONF_SAMPLE_ACCL_RATE]))
    cg.add(var.set_sample_gyro_rate(config[CONF_SAMPLE_GYRO_RATE]))
    cg.add(var.set_power_save(config[CONF_POWER_SAVE]))

    for d in ["x", "y", "z"]:
        accel_key = f"accel_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f"set_accel_{d}_sensor")(sens))
        accel_key = f"gyro_{d}"
        if accel_key in config:
            sens = await sensor.new_sensor(config[accel_key])
            cg.add(getattr(var, f"set_gyro_{d}_sensor")(sens))

    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
