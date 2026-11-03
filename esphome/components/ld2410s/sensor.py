import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    DEVICE_CLASS_DISTANCE,
    ICON_SIGNAL_DISTANCE_VARIANT,
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
)
from esphome.types import ConfigType

from . import CONF_LD2410S_ID, LD2410SComponent

DEPENDENCIES = ["ld2410s"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410SComponent),
    cv.Required(CONF_DISTANCE): sensor.sensor_schema(
        unit_of_measurement=UNIT_CENTIMETER,
        icon=ICON_SIGNAL_DISTANCE_VARIANT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_DISTANCE, hub.set_distance_sensor)
