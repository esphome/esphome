from collections.abc import Callable
import math
from typing import Any

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_ID, CONF_THRESHOLD, CONF_TYPE

from .. import (
    CONF_MOTION_ID,
    MotionComponent,
    check_has_accelerometer,
    check_update_interval,
    motion_ns,
)

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
    default_threshold: float,
    threshold_validator: Callable[[Any], Any],
    default_duration: str | None = None,
) -> cv.Schema:
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
# 0 is excluded: cos(0) == 1.0 would make the C++ comparison always false, so
# face_up/face_down would never trigger.
_angle_threshold = cv.float_range(min=0.0, max=90.0, min_included=False)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "face_up": _binary_sensor_schema(30.0, _angle_threshold),
        "face_down": _binary_sensor_schema(30.0, _angle_threshold),
        "free_fall": _binary_sensor_schema(0.15, cv.positive_float, "100ms"),
        "moving": _binary_sensor_schema(0.05, cv.positive_float, "2s"),
    }
)

# These types detect brief motion events, so they need frequent samples;
# face_up/face_down track a steady orientation and aren't time-sensitive.
_FAST_DETECTION_TYPES = ("free_fall", "moving")

# face_up/face_down/free_fall are entirely accelerometer-driven; "moving" is exempt
# since it detects motion from either the accelerometer or the gyroscope.
_ACCEL_ONLY_TYPES = ("face_up", "face_down", "free_fall")


def _final_validate(config: dict) -> None:
    sensor_type = config[CONF_TYPE]
    if sensor_type in _FAST_DETECTION_TYPES:
        check_update_interval(config[CONF_MOTION_ID], sensor_type.replace("_", "-"))
    if sensor_type in _ACCEL_ONLY_TYPES:
        check_has_accelerometer(config[CONF_MOTION_ID], sensor_type.replace("_", "-"))


FINAL_VALIDATE_SCHEMA = _final_validate


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
