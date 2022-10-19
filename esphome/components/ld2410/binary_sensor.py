import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_PRESENCE,
)
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_HAS_TARGET = "has_target"
CONF_HASMOVING_TARGET = "has_moving_target"
CONF_HASSTILL_TARGET = "has_still_target"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_HAS_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE
    ),
    cv.Optional(CONF_HASMOVING_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION
    ),
    cv.Optional(CONF_HASSTILL_TARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_HAS_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HAS_TARGET])
        cg.add(ld2410_component.settargetsensor(sens))
    if CONF_HASMOVING_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HASMOVING_TARGET])
        cg.add(ld2410_component.setmovingtargetsensor(sens))
    if CONF_HASSTILL_TARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HASSTILL_TARGET])
        cg.add(ld2410_component.setstilltargetsensor(sens))
