import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HAS_MOVING_TARGET,
    CONF_HAS_STILL_TARGET,
    CONF_HAS_TARGET,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
)

from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]

ICON_MEDITATION = "mdi:meditation"
ICON_SHIELD_ACCOUNT = "mdi:shield-account"
ICON_TARGET_ACCOUNT = "mdi:target-account"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon=ICON_SHIELD_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_MOVING_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        icon=ICON_TARGET_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_STILL_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon=ICON_MEDITATION,
    ),
}


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if has_target_config := config.get(CONF_HAS_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_target_config)
        cg.add(ld2450_component.set_target_binary_sensor(sens))
    if has_moving_target_config := config.get(CONF_HAS_MOVING_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_moving_target_config)
        cg.add(ld2450_component.set_moving_target_binary_sensor(sens))
    if has_still_target_config := config.get(CONF_HAS_STILL_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_still_target_config)
        cg.add(ld2450_component.set_still_target_binary_sensor(sens))
