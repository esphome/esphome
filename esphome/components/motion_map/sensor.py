"""Sensor platform for Motion Map component - CSI feature sensors."""
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)

from . import CONF_MOTION_MAP_ID, MotionMapComponent, motion_map_ns

DEPENDENCIES = ["motion_map"]

# Sensor types for CSI features
CONF_VARIANCE = "variance"
CONF_AMPLITUDE = "amplitude"
CONF_ENTROPY = "entropy"
CONF_SKEWNESS = "skewness"

MotionMapSensor = motion_map_ns.class_(
    "MotionMapSensor", sensor.Sensor, cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MOTION_MAP_ID): cv.use_id(MotionMapComponent),
        cv.Optional(CONF_VARIANCE): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_AMPLITUDE): sensor.sensor_schema(
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_ENTROPY): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_SKEWNESS): sensor.sensor_schema(
            accuracy_decimals=3,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    """Generate code for the motion map sensors."""
    parent = await cg.get_variable(config[CONF_MOTION_MAP_ID])

    if variance_config := config.get(CONF_VARIANCE):
        sens = await sensor.new_sensor(variance_config)
        cg.add(parent.set_variance_sensor(sens))

    if amplitude_config := config.get(CONF_AMPLITUDE):
        sens = await sensor.new_sensor(amplitude_config)
        cg.add(parent.set_amplitude_sensor(sens))

    if entropy_config := config.get(CONF_ENTROPY):
        sens = await sensor.new_sensor(entropy_config)
        cg.add(parent.set_entropy_sensor(sens))

    if skewness_config := config.get(CONF_SKEWNESS):
        sens = await sensor.new_sensor(skewness_config)
        cg.add(parent.set_skewness_sensor(sens))
