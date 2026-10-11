import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HAS_TARGET,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_MOTION_SENSOR,
)
from esphome.types import ConfigType

from . import CONF_LD2410S_ID, LD2410S

DEPENDENCIES = ["ld2410s"]

CONF_HAS_CALIBRATION_RUNNING = "has_calibration_running"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Optional(CONF_HAS_CALIBRATION_RUNNING): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_RUNNING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2410S_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_HAS_TARGET, hub.set_presence_binary_sensor)
    await binary_sensors(
        CONF_HAS_CALIBRATION_RUNNING, hub.set_calibration_running_binary_sensor
    )
