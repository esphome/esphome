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


def _binary_sensor_schema(default_threshold, default_duration=None):
    schema = (
        binary_sensor.binary_sensor_schema(MotionBinarySensor)
        .extend(
            {
                cv.GenerateID(CONF_MOTION_ID): cv.use_id(MotionComponent),
                cv.Optional(CONF_THRESHOLD, default=default_threshold): cv.float_,
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


CONFIG_SCHEMA = cv.typed_schema(
    {
        "face_up": _binary_sensor_schema(0.85),
        "face_down": _binary_sensor_schema(0.85),
        "free_fall": _binary_sensor_schema(0.15, "100ms"),
        "moving": _binary_sensor_schema(0.05, "2s"),
    }
)


async def to_code(config):
    sensor_type = config[CONF_TYPE]
    parent = await cg.get_variable(config[CONF_MOTION_ID])

    var = cg.new_Pvariable(config[CONF_ID], parent, SENSOR_TYPES[sensor_type])
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    if CONF_DURATION in config:
        cg.add(var.set_duration(config[CONF_DURATION]))
