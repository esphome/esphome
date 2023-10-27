import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PRESENCE,
    ICON_MOTION_SENSOR,
)

from . import CONF_LD2450_ID, LD2450Component

DEPENDENCIES = ["ld2450"]
CONF_ANY_PRESENCE = "any_presence"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
    cv.Optional(CONF_ANY_PRESENCE): binary_sensor.binary_sensor_schema(
        device_class=DEVICE_CLASS_PRESENCE, icon=ICON_MOTION_SENSOR
    ),
}


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if any_presence := config.get(CONF_ANY_PRESENCE):
        sens = await binary_sensor.new_binary_sensor(any_presence)
        cg.add(ld2450_component.set_any_presence_binary_sensor(sens))
