"""Resonate Sensor Setup."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)

from .. import CONF_RESONATE_ID, ResonateHub, resonate_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["resonate"]

ResonateSensor = resonate_ns.class_(
    "ResonateSensor",
    sensor.Sensor,
    cg.Component,
)

ResonateSensorTypes = resonate_ns.enum("ResonateSensorTypes", is_class=True)
RESONATE_SENSOR_TYPES = {
    "kalman_error": ResonateSensorTypes.KALMAN_ERROR,
    "audible_syncs": ResonateSensorTypes.AUDIBLE_SYNCS,
    "hard_sync_frames_added": ResonateSensorTypes.HARD_SYNC_FRAMES_ADDED,
    "hard_sync_frames_removed": ResonateSensorTypes.HARD_SYNC_FRAMES_REMOVED,
    "single_sync_frames_added": ResonateSensorTypes.SINGLE_SYNC_FRAMES_ADDED,
    "single_sync_frames_removed": ResonateSensorTypes.SINGLE_SYNC_FRAMES_REMOVED,
}


CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(): cv.declare_id(ResonateSensor),
        cv.GenerateID(CONF_RESONATE_ID): cv.use_id(ResonateHub),
        cv.Required(CONF_TYPE): cv.enum(RESONATE_SENSOR_TYPES),
    }
)


async def to_code(config):
    cg.add_define("USE_RESONATE_SENSOR", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_RESONATE_ID])
    await sensor.register_sensor(var, config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))
