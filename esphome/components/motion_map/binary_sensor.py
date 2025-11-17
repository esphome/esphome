"""Binary sensor platform for Motion Map component."""
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_MOTION,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from . import CONF_MOTION_MAP_ID, MotionMapComponent, motion_map_ns

DEPENDENCIES = ["motion_map"]

MotionMapBinarySensor = motion_map_ns.class_(
    "MotionMapBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(
    MotionMapBinarySensor,
    device_class=DEVICE_CLASS_MOTION,
).extend(
    {
        cv.GenerateID(CONF_MOTION_MAP_ID): cv.use_id(MotionMapComponent),
    }
)


async def to_code(config):
    """Generate code for the motion binary sensor."""
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_MOTION_MAP_ID])
    cg.add(parent.set_motion_binary_sensor(var))
