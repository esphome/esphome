import math

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID, CONF_THRESHOLD, CONF_TYPE

from . import CONF_MOTION_ID, MotionComponent, motion_ns

DEPENDENCIES = ["motion"]

MotionBinarySensor = motion_ns.class_(
    "MotionBinarySensor", binary_sensor.BinarySensor, cg.Component
)

MotionBinarySensorType = motion_ns.enum("MotionBinarySensorType")

SENSOR_TYPES = {
    "face_up": MotionBinarySensorType.MOTION_BINARY_SENSOR_FACE_UP,
    "face_down": MotionBinarySensorType.MOTION_BINARY_SENSOR_FACE_DOWN,
    "free_fall": MotionBinarySensorType.MOTION_BINARY_SENSOR_FREE_FALL,
    "moving": MotionBinarySensorType.MOTION_BINARY_SENSOR_MOVING,
}

# face_up / face_down configure their threshold as a maximum tilt angle in degrees;
# the C++ side compares against the cosine of that angle.
ANGLE_THRESHOLD_TYPES = ("face_up", "face_down")


def _binary_sensor_schema(
    default_threshold, threshold_validator, default_duration=None
):
    schema = (
        binary_sensor.binary_sensor_schema(MotionBinarySensor)
        .extend(
            {
                cv.GenerateID(CONF_MOTION_ID): cv.use_id(MotionComponent),
                cv.Optional(
                    CONF_THRESHOLD, default=default_threshold
                ): threshold_validator,
            }
        )
        .extend(cv.COMPONENT_SCHEMA)
    )

    if default_duration is not None:
        schema = schema.extend(
            {
                cv.Optional(
                    CONF_DURATION, default=default_duration
                ): cv.positive_time_period_milliseconds,
            }
        )
    return schema


# Tilt angle in degrees, from horizontal, within which the device counts as face up/down.
_angle_threshold = cv.float_range(min=0.0, max=90.0)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "face_up": _binary_sensor_schema(30.0, _angle_threshold),
        "face_down": _binary_sensor_schema(30.0, _angle_threshold),
        "free_fall": _binary_sensor_schema(0.15, cv.float_, "100ms"),
        "moving": _binary_sensor_schema(0.05, cv.float_, "2s"),
    }
)


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    parent = await cg.get_variable(config[CONF_MOTION_ID])

    var = cg.new_Pvariable(config[CONF_ID], parent, SENSOR_TYPES[sensor_type])
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)

    threshold = config[CONF_THRESHOLD]
    if sensor_type in ANGLE_THRESHOLD_TYPES:
        # Convert the configured tilt angle (degrees) to the cosine the C++ side expects.
        threshold = round(math.cos(math.radians(threshold)), 6)
    cg.add(var.set_threshold(threshold))
    if CONF_DURATION in config:
        cg.add(var.set_duration(config[CONF_DURATION]))
