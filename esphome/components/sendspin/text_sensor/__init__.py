"""Sendspin Text Sensor Setup."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

from .. import CONF_SENDSPIN_ID, SendspinHub, sendspin_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["sendspin"]

SendspinTextSensor = sendspin_ns.class_(
    "SendspinTextSensor",
    text_sensor.TextSensor,
    cg.Component,
)

SendspinMetadataTypes = sendspin_ns.enum("SendspinMetadataTypes", is_class=True)
SENDSPIN_METADATA_TYPES = {
    "title": SendspinMetadataTypes.TITLE,
    "artist": SendspinMetadataTypes.ARTIST,
    "album": SendspinMetadataTypes.ALBUM,
    "album_artist": SendspinMetadataTypes.ALBUM_ARTIST,
    "year": SendspinMetadataTypes.YEAR,
    "track": SendspinMetadataTypes.TRACK,
}


CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(SendspinTextSensor),
        cv.GenerateID(CONF_SENDSPIN_ID): cv.use_id(SendspinHub),
        cv.Required(CONF_TYPE): cv.enum(SENDSPIN_METADATA_TYPES),
    }
)


async def to_code(config):
    cg.add_define("USE_SENDSPIN_METADATA", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_SENDSPIN_ID])
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_metadata_string_type(config[CONF_TYPE]))
