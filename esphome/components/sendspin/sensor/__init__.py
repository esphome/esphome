"""Sendspin Sensor Setup."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
)

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinSensor = sendspin_ns.class_(
    "SendspinSensor",
    sensor.Sensor,
    cg.Component,
)

SendspinSensorTypes = sendspin_ns.enum("SendspinSensorTypes", is_class=True)
SENDSPIN_SENSOR_TYPES = {
    "kalman_error": SendspinSensorTypes.KALMAN_ERROR,
    "audible_syncs": SendspinSensorTypes.AUDIBLE_SYNCS,
    "hard_sync_frames_added": SendspinSensorTypes.HARD_SYNC_FRAMES_ADDED,
    "hard_sync_frames_removed": SendspinSensorTypes.HARD_SYNC_FRAMES_REMOVED,
    "single_sync_frames_added": SendspinSensorTypes.SINGLE_SYNC_FRAMES_ADDED,
    "single_sync_frames_removed": SendspinSensorTypes.SINGLE_SYNC_FRAMES_REMOVED,
}


CONFIG_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
).extend(
    {
        cv.GenerateID(): cv.declare_id(SendspinSensor),
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
        cv.Required(CONF_TYPE): cv.enum(SENDSPIN_SENSOR_TYPES),
    }
)


async def to_code(config):
    cg.add_define("USE_SENDSPIN_SENSOR", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await sensor.register_sensor(var, config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))
