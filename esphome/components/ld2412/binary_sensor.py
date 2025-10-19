import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HAS_MOVING_TARGET,
    CONF_HAS_STILL_TARGET,
    CONF_HAS_TARGET,
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_RUNNING,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_ACCOUNT,
    ICON_MOTION_SENSOR,
)

from . import CONF_LD2412_ID, LD2412Component

DEPENDENCIES = ["ld2412"]

CONF_DYNAMIC_BACKGROUND_CORRECTION_STATUS = "dynamic_background_correction_status"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2412_ID): cv.use_id(LD2412Component),
    cv.Optional(
        CONF_DYNAMIC_BACKGROUND_CORRECTION_STATUS
    ): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_RUNNING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon=ICON_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_ACCOUNT,
    ),
    cv.Optional(CONF_HAS_MOVING_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_MOTION_SENSOR,
    ),
    cv.Optional(CONF_HAS_STILL_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        filters=[{"settle": cv.TimePeriod(milliseconds=1000)}],
        icon=ICON_MOTION_SENSOR,
    ),
}


async def to_code(config):
    LD2412_component = await cg.get_variable(config[CONF_LD2412_ID])
    if dynamic_background_correction_status_config := config.get(
        CONF_DYNAMIC_BACKGROUND_CORRECTION_STATUS
    ):
        sens = await binary_sensor.new_binary_sensor(
            dynamic_background_correction_status_config
        )
        cg.add(
            LD2412_component.set_dynamic_background_correction_status_binary_sensor(
                sens
            )
        )
    if has_target_config := config.get(CONF_HAS_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_target_config)
        cg.add(LD2412_component.set_target_binary_sensor(sens))
    if has_moving_target_config := config.get(CONF_HAS_MOVING_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_moving_target_config)
        cg.add(LD2412_component.set_moving_target_binary_sensor(sens))
    if has_still_target_config := config.get(CONF_HAS_STILL_TARGET):
        sens = await binary_sensor.new_binary_sensor(has_still_target_config)
        cg.add(LD2412_component.set_still_target_binary_sensor(sens))
