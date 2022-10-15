import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_MOTION,
    DEVICE_CLASS_PRESENCE,
)
from . import CONF_LD2410_ID, LD2410Component

DEPENDENCIES = ["ld2410"]
CONF_HASTARGET = "has_target"
CONF_HASMOVINGTARGET = "has_moving_target"
CONF_HASSTILLTARGET = "has_still_target"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410_ID): cv.use_id(LD2410Component),
    cv.Optional(CONF_HASTARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE
    ),
    cv.Optional(CONF_HASMOVINGTARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_MOTION
    ),
    cv.Optional(CONF_HASSTILLTARGET): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE
    ),
}


async def to_code(config):
    ld2410_component = await cg.get_variable(config[CONF_LD2410_ID])
    if CONF_HASTARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HASTARGET])
        cg.add(ld2410_component.settargetsensor(sens))
    if CONF_HASMOVINGTARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HASMOVINGTARGET])
        cg.add(ld2410_component.setmovingtargetsensor(sens))
    if CONF_HASSTILLTARGET in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HASSTILLTARGET])
        cg.add(ld2410_component.setstilltargetsensor(sens))
