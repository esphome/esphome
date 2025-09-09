import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.canbus import CONF_CANBUS_ID
import esphome.config_validation as cv
from esphome.const import CONF_STATE, ENTITY_CATEGORY_DIAGNOSTIC

from . import esp32_can

DEPENDENCIES = ["canbus"]

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_CANBUS_ID): cv.use_id(esp32_can),
    cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC
    ),
}


async def to_code(config):
    esp32can = await cg.get_variable(config[CONF_CANBUS_ID])
    if CONF_STATE in config:
        var = await text_sensor.new_text_sensor(config[CONF_STATE])
        func = getattr(esp32can, f"set_{CONF_STATE}_text_sensor")
        cg.add(func(var))
