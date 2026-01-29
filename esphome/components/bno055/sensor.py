import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_BRIEFCASE_DOWNLOAD,
    ICON_SCREEN_ROTATION,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_METER_PER_SECOND_SQUARED,
    UNIT_MICROTESLA,
)

DEPENDENCIES = ["i2c"]

# Sensor configuration keys
CONF_ACCEL_X = "accel_x"
CONF_ACCEL_Y = "accel_y"
CONF_ACCEL_Z = "accel_z"
CONF_GYRO_X = "gyro_x"
CONF_GYRO_Y = "gyro_y"
CONF_GYRO_Z = "gyro_z"
CONF_MAG_X = "mag_x"
CONF_MAG_Y = "mag_y"
CONF_MAG_Z = "mag_z"
CONF_EULER_HEADING = "euler_heading"
CONF_EULER_ROLL = "euler_roll"
CONF_EULER_PITCH = "euler_pitch"
CONF_QUATERNION_W = "quaternion_w"
CONF_QUATERNION_X = "quaternion_x"
CONF_QUATERNION_Y = "quaternion_y"
CONF_QUATERNION_Z = "quaternion_z"
CONF_LINEAR_ACCEL_X = "linear_accel_x"
CONF_LINEAR_ACCEL_Y = "linear_accel_y"
CONF_LINEAR_ACCEL_Z = "linear_accel_z"
CONF_GRAVITY_X = "gravity_x"
CONF_GRAVITY_Y = "gravity_y"
CONF_GRAVITY_Z = "gravity_z"

UNIT_DEGREES = "°"

bno055_ns = cg.esphome_ns.namespace("bno055")
BNO055Component = bno055_ns.class_(
    "BNO055Component", cg.PollingComponent, i2c.I2CDevice
)

# Sensor schemas for different measurement types
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

mag_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_MICROTESLA,
    icon="mdi:magnet",
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

euler_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_DEGREES,
    icon="mdi:axis-arrow",
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)

quaternion_schema = sensor.sensor_schema(
    unit_of_measurement="",
    icon="mdi:axis-arrow",
    accuracy_decimals=4,
    state_class=STATE_CLASS_MEASUREMENT,
)

temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BNO055Component),
            # Euler angles
            cv.Optional(CONF_EULER_HEADING): euler_schema,
            cv.Optional(CONF_EULER_ROLL): euler_schema,
            cv.Optional(CONF_EULER_PITCH): euler_schema,
            # Quaternion
            cv.Optional(CONF_QUATERNION_W): quaternion_schema,
            cv.Optional(CONF_QUATERNION_X): quaternion_schema,
            cv.Optional(CONF_QUATERNION_Y): quaternion_schema,
            cv.Optional(CONF_QUATERNION_Z): quaternion_schema,
            # Linear acceleration (without gravity)
            cv.Optional(CONF_LINEAR_ACCEL_X): accel_schema,
            cv.Optional(CONF_LINEAR_ACCEL_Y): accel_schema,
            cv.Optional(CONF_LINEAR_ACCEL_Z): accel_schema,
            # Gravity vector
            cv.Optional(CONF_GRAVITY_X): accel_schema,
            cv.Optional(CONF_GRAVITY_Y): accel_schema,
            cv.Optional(CONF_GRAVITY_Z): accel_schema,
            # Raw accelerometer
            cv.Optional(CONF_ACCEL_X): accel_schema,
            cv.Optional(CONF_ACCEL_Y): accel_schema,
            cv.Optional(CONF_ACCEL_Z): accel_schema,
            # Gyroscope
            cv.Optional(CONF_GYRO_X): gyro_schema,
            cv.Optional(CONF_GYRO_Y): gyro_schema,
            cv.Optional(CONF_GYRO_Z): gyro_schema,
            # Magnetometer
            cv.Optional(CONF_MAG_X): mag_schema,
            cv.Optional(CONF_MAG_Y): mag_schema,
            cv.Optional(CONF_MAG_Z): mag_schema,
            # Temperature
            cv.Optional(CONF_TEMPERATURE): temperature_schema,
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(0x29))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # Euler angles
    if CONF_EULER_HEADING in config:
        sens = await sensor.new_sensor(config[CONF_EULER_HEADING])
        cg.add(var.set_euler_heading_sensor(sens))
    if CONF_EULER_ROLL in config:
        sens = await sensor.new_sensor(config[CONF_EULER_ROLL])
        cg.add(var.set_euler_roll_sensor(sens))
    if CONF_EULER_PITCH in config:
        sens = await sensor.new_sensor(config[CONF_EULER_PITCH])
        cg.add(var.set_euler_pitch_sensor(sens))

    # Quaternion
    if CONF_QUATERNION_W in config:
        sens = await sensor.new_sensor(config[CONF_QUATERNION_W])
        cg.add(var.set_quaternion_w_sensor(sens))
    if CONF_QUATERNION_X in config:
        sens = await sensor.new_sensor(config[CONF_QUATERNION_X])
        cg.add(var.set_quaternion_x_sensor(sens))
    if CONF_QUATERNION_Y in config:
        sens = await sensor.new_sensor(config[CONF_QUATERNION_Y])
        cg.add(var.set_quaternion_y_sensor(sens))
    if CONF_QUATERNION_Z in config:
        sens = await sensor.new_sensor(config[CONF_QUATERNION_Z])
        cg.add(var.set_quaternion_z_sensor(sens))

    # Linear acceleration
    for d in ["x", "y", "z"]:
        key = f"linear_accel_{d}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_linear_accel_{d}_sensor")(sens))

    # Gravity
    for d in ["x", "y", "z"]:
        key = f"gravity_{d}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_gravity_{d}_sensor")(sens))

    # Raw accelerometer
    for d in ["x", "y", "z"]:
        key = f"accel_{d}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_accel_{d}_sensor")(sens))

    # Gyroscope
    for d in ["x", "y", "z"]:
        key = f"gyro_{d}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_gyro_{d}_sensor")(sens))

    # Magnetometer
    for d in ["x", "y", "z"]:
        key = f"mag_{d}"
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_mag_{d}_sensor")(sens))

    # Temperature
    if CONF_TEMPERATURE in config:
        sens = await sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
