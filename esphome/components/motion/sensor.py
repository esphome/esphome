#  YAML config keys
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    ICON_ACCELERATION,
    ICON_ROTATE_RIGHT,
    STATE_CLASS_MEASUREMENT,
    UNIT_DEGREE_PER_SECOND,
    UNIT_DEGREES,
    UNIT_G,
)
from esphome.cpp_generator import MockObj
from esphome.cpp_types import std_ns
import esphome.final_validate as fv

from . import (
    AXES,
    CONF_MOTION_ID,
    KEY_ACCELEROMETER,
    KEY_GYROSCOPE,
    SENSOR_SCHEMA,
    motion_ns,
)

MotionData = motion_ns.class_("MotionData")

CONF_PITCH = "pitch"
CONF_ROLL = "roll"
ICON_SEESAW = "mdi:seesaw"


def _accel_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_G,
        icon=ICON_ACCELERATION,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(SENSOR_SCHEMA)


def _gyro_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREE_PER_SECOND,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(SENSOR_SCHEMA)


def _level_sensor_schema():
    return sensor.sensor_schema(
        unit_of_measurement=UNIT_DEGREES,
        icon=ICON_SEESAW,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ).extend(SENSOR_SCHEMA)


_ACCELERATIONS = ["acceleration_" + a for a in AXES]
_GYROSCOPES = ["gyroscope_" + g for g in AXES]
_ANGULAR_RATES = ["angular_rate_" + r for r in AXES]

CONFIG_SCHEMA = cv.typed_schema(
    {
        **{x: _accel_sensor_schema() for x in _ACCELERATIONS},
        **{x: _gyro_sensor_schema() for x in _GYROSCOPES},
        **{x: _gyro_sensor_schema() for x in _ANGULAR_RATES},
        **{x: _level_sensor_schema() for x in (CONF_PITCH, CONF_ROLL)},
    }
)


def _final_validate(config: dict) -> None:
    full_config = fv.full_config.get()
    motion_path = full_config.get_path_for_id(config[CONF_MOTION_ID])[:-1]
    motion_config = full_config.get_config_for_path(motion_path)
    has_accel = motion_config.get(KEY_ACCELEROMETER, False)
    has_gyro = motion_config.get(KEY_GYROSCOPE, False)

    sensor_type = config[CONF_TYPE]
    if (
        sensor_type in _ACCELERATIONS or sensor_type in (CONF_ROLL, CONF_PITCH)
    ) and not has_accel:
        raise cv.Invalid(
            "The motion device does not measure acceleration", path=[CONF_TYPE]
        )
    if (sensor_type in _GYROSCOPES or sensor_type in _ANGULAR_RATES) and not has_gyro:
        raise cv.Invalid(
            "The motion device does not measure angular rate", path=[CONF_TYPE]
        )


FINAL_VALIDATE_SCHEMA = _final_validate


def build_sensor_expr(sensor_type: str, data: MockObj) -> MockObj:
    """Build the C++ expression for a motion sensor type."""

    # Note that <numbers> is included via this component's header file.
    pif = std_ns.namespace("numbers").pi_v.template(cg.float_)
    if sensor_type == CONF_ROLL:
        ay = data.acceleration[1]
        az = data.acceleration[2]
        return std_ns.atan2(ay, az) * (180.0 / pif)
    if sensor_type == CONF_PITCH:
        ax = data.acceleration[0]
        ay = data.acceleration[1]
        az = data.acceleration[2]
        return std_ns.atan2(-ax, std_ns.sqrt(ay * ay + az * az)) * (180.0 / pif)
    sensor_offset = AXES.index(sensor_type[-1:])
    if sensor_type in _GYROSCOPES:
        sensor_type = _ANGULAR_RATES[sensor_offset]
    return getattr(data, str(sensor_type[:-2]))[sensor_offset]


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_MOTION_ID])
    data = MockObj("data")
    expr = build_sensor_expr(sensor_type, data)
    value_lambda = await cg.process_lambda(
        var.publish_state(expr),
        [(MotionData.operator("ref"), str(data))],
    )
    cg.add(parent.add_listener(value_lambda))
