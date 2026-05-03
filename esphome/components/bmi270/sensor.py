#  YAML config keys
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_TEMPERATURE,
    ICON_ACCELERATION,
    ICON_ROTATE_RIGHT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_DEGREE_PER_SECOND,
    UNIT_DEGREES,
    UNIT_G,
)
from esphome.cpp_generator import MockObj
from esphome.cpp_types import std_ns

from . import AXES, CONF_BMI270_ID, SENSOR_SCHEMA, BMI270AccelData


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


CONF_PITCH = "pitch"
CONF_ROLL = "roll"
ICON_SEESAW = "mdi:seesaw"

_ACCELERATIONS = ["acceleration_" + a for a in AXES]
_GYROSCOPES = ["gyroscope_" + g for g in AXES]
_ANGULAR_RATES = ["angular_rate_" + r for r in AXES]

CONFIG_SCHEMA = cv.typed_schema(
    {
        **{x: _accel_sensor_schema() for x in _ACCELERATIONS},
        **{x: _gyro_sensor_schema() for x in _GYROSCOPES},
        **{x: _gyro_sensor_schema() for x in _ANGULAR_RATES},
        **{x: _level_sensor_schema() for x in (CONF_PITCH, CONF_ROLL)},
        # Temperature
        CONF_TEMPERATURE: sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            icon=ICON_THERMOMETER,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ).extend(SENSOR_SCHEMA),
    }
)


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    var = await sensor.new_sensor(config)
    parent = await cg.get_variable(config[CONF_BMI270_ID])
    data = MockObj("data")
    pif = std_ns.namespace("numbers").pi_v.template(cg.float_)
    if sensor_type == CONF_TEMPERATURE:
        expr = data.temperature
    elif sensor_type == CONF_ROLL:
        ay = data.acceleration[1]
        az = data.acceleration[2]
        expr = std_ns.atan2f(ay, az) * (180.0 / pif)
    elif sensor_type == CONF_PITCH:
        ax = data.acceleration[0]
        ay = data.acceleration[1]
        az = data.acceleration[2]
        expr = std_ns.atan2f(-ax, std_ns.sqrtf(ay * ay + az * az)) * (180.0 / pif)
    else:
        sensor_offset = AXES.index(sensor_type[-1:])
        if sensor_type in _GYROSCOPES:
            sensor_type = _ANGULAR_RATES[sensor_offset]
        expr = getattr(data, str(sensor_type[:-2]))[sensor_offset]
    value_lambda = await cg.process_lambda(
        var.publish_state(expr),
        [(BMI270AccelData.operator("ref"), "data")],
    )
    cg.add(parent.add_listener(value_lambda))
