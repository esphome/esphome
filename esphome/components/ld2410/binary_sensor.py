import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_PRESENCE,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_HAS_TARGET = "has_target"
CONF_HAS_MOVING_TARGET = "has_moving_target"
CONF_HAS_STILL_TARGET = "has_still_target"
CONF_OUT_PIN_PRESENCE_STATUS = "out_pin_presence_status"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon="mdi:eye",
    ),
    cv.Optional(CONF_HAS_MOVING_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION,
        icon="mdi:motion-sensor",
    ),
    cv.Optional(CONF_HAS_STILL_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_OCCUPANCY,
        icon="mdi:motion-sensor",
    ),
    cv.Optional(CONF_OUT_PIN_PRESENCE_STATUS): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:eye",
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_HAS_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_TARGET])
        cg.add(ld2410_component.set_target_binary_sensor(sens))
    if CONF_HAS_MOVING_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_MOVING_TARGET])
        cg.add(ld2410_component.set_moving_target_binary_sensor(sens))
    if CONF_HAS_STILL_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_STILL_TARGET])
        cg.add(ld2410_component.set_still_target_binary_sensor(sens))
    if CONF_OUT_PIN_PRESENCE_STATUS in config:
        sens = await binary_sensor.new_binary_sensor(
            config[CONF_OUT_PIN_PRESENCE_STATUS]
        )
        cg.add(ld2410_component.set_out_pin_presence_status_binary_sensor(sens))
