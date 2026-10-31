import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MOVING_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
)
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
        cv.Optional(CONF_MOVING_DISTANCE): sensor.sensor_schema(
            device_class=DEVICE_CLASS_DISTANCE,
            unit_of_measurement=UNIT_CENTIMETER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_MOVING_DISTANCE, hub.set_distance_sensor)
