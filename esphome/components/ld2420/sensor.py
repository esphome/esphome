import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    UNIT_CENTIMETER,
)
from . import CONF_LD2420_ID, LD2420Component

DEPENDENCIES = ["ld2420"]
CONF_MOVING_DISTANCE = "moving_distance"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
    cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
        device_class=DEVICE_CLASS_DISTANCE, unit_of_measurement=UNIT_CENTIMETER
    ),
}


async def to_code(config):
    ld2420_component = await cg.get_variable(config[CONF_LD2420_ID])
    if CONF_MOVING_DISTANCE in config:
        sens = await sensor.new_sensor(config[CONF_MOVING_DISTANCE])
        cg.add(ld2420_component.set_moving_target_distance_sensor(sens))
