import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
)
from . import CONF_ABBAURORA_ID, ABBAURORA_COMPONENT_SCHEMA

CONF_VERSION = "version"
CONF_STATUS = "connection_status"
CONF_IDENTIFICATION = "identification"
AUTO_LOAD = ["abbaurora"]

TYPES = {
    CONF_VERSION,
    CONF_STATUS,
    CONF_IDENTIFICATION,
}

CONFIG_SCHEMA = ABBAURORA_COMPONENT_SCHEMA.extend(
    {
        cv.Optional( type ): text_sensor.TEXT_SENSOR_SCHEMA.extend( { cv.GenerateID(): cv.declare_id(text_sensor.TextSensor) } )
        for type in TYPES
    }
)

async def to_code(config):
    paren = await cg.get_variable(config[CONF_ABBAURORA_ID])

    for type in TYPES:
        if type in config:
            conf = config[type]
            var = cg.new_Pvariable(conf[CONF_ID])
            await text_sensor.register_text_sensor(var, conf)
            cg.add(getattr(paren, f"set_{type}_text_sensor")(var))
