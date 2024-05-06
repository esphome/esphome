import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_VIBRATIONS,
    DEVICE_CLASS_VIBRATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ICON_VIBRATE,
)
from . import CONF_GDK101_ID, GDK101Component

DEPENDENCIES = ["gdk101"]

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_GDK101_ID): cv.use_id(GDK101Component),
        cv.Required(CONF_VIBRATIONS): binary_sensor.binary_sensor_schema(
            device_class=DEVICE_CLASS_VIBRATION,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_VIBRATE,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_GDK101_ID])
    var = await binary_sensor.new_binary_sensor(config[CONF_VIBRATIONS])
    cg.add(hub.set_vibration_binary_sensor(var))
