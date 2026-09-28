import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HAS_MOVING_TARGET,
    CONF_HAS_STILL_TARGET,
    CONF_HAS_TARGET,
    CONF_ID,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
)
from esphome.types import ConfigType

from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]

ICON_MEDITATION = "mdi:meditation"
ICON_SHIELD_ACCOUNT = "mdi:shield-account"
ICON_TARGET_ACCOUNT = "mdi:target-account"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_SHIELD_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_MOVING_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_TARGET_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_STILL_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_MEDITATION,
    ),
}


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    binary_sensors = binary_sensor.sub_binary_sensors(config)
    await binary_sensors(CONF_HAS_TARGET, hub.set_target_binary_sensor)
    await binary_sensors(CONF_HAS_MOVING_TARGET, hub.set_moving_target_binary_sensor)
    await binary_sensors(CONF_HAS_STILL_TARGET, hub.set_still_target_binary_sensor)
