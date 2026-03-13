"""Sendspin Sensor Setup."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_TYPE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLISECOND,
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
SENDSPIN_DIAGNOSTIC_SENSOR_TYPES = {
    "kalman_error": SendspinSensorTypes.KALMAN_ERROR,
}

SENDSPIN_METADATA_SENSOR_TYPES = {
    "track_progress": SendspinSensorTypes.TRACK_PROGRESS,
    "track_duration": SendspinSensorTypes.TRACK_DURATION,
}

SENDSPIN_SENSOR_TYPES = {
    **SENDSPIN_DIAGNOSTIC_SENSOR_TYPES,
    **SENDSPIN_METADATA_SENSOR_TYPES,
}


def _sensor_schema(value):
    """Create the appropriate sensor schema based on the sensor type."""
    sensor_type = value.get(CONF_TYPE)
    if sensor_type in SENDSPIN_METADATA_SENSOR_TYPES:
        schema = sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            unit_of_measurement=UNIT_MILLISECOND,
        )
    else:
        schema = sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
    schema = schema.extend(
        {
            cv.GenerateID(): cv.declare_id(SendspinSensor),
            cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
            cv.Required(CONF_TYPE): cv.enum(SENDSPIN_SENSOR_TYPES),
        }
    )
    return schema(value)


CONFIG_SCHEMA = _sensor_schema


async def to_code(config):
    cg.add_define("USE_SENDSPIN_SENSOR", True)

    if config[CONF_TYPE] in SENDSPIN_METADATA_SENSOR_TYPES.values():
        cg.add_define("USE_SENDSPIN_METADATA", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await sensor.register_sensor(var, config)

    cg.add(var.set_sensor_type(config[CONF_TYPE]))
