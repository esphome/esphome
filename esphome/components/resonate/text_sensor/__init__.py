"""Resonate Text Sensor Setup."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

from .. import CONF_RESONATE_ID, ResonateHub, resonate_ns

CODEOWNERS = ["@kahrendt"]
DEPENDENCIES = ["resonate"]

ResonateTextSensor = resonate_ns.class_(
    "ResonateTextSensor",
    text_sensor.TextSensor,
    cg.Component,
)

ResonateMetadataTypes = resonate_ns.enum("ResonateMetadataTypes", is_class=True)
RESONATE_METADATA_TYPES = {
    "title": ResonateMetadataTypes.TITLE,
    "artist": ResonateMetadataTypes.ARTIST,
    "album": ResonateMetadataTypes.ALBUM,
    "year": ResonateMetadataTypes.YEAR,
    "track": ResonateMetadataTypes.TRACK,
}


CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(ResonateTextSensor),
        cv.GenerateID(CONF_RESONATE_ID): cv.use_id(ResonateHub),
        cv.Required(CONF_TYPE): cv.enum(RESONATE_METADATA_TYPES),
    }
)


async def to_code(config):
    cg.add_define("USE_RESONATE_METADATA", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_RESONATE_ID])
    await text_sensor.register_text_sensor(var, config)

    cg.add(var.set_metadata_string_type(config[CONF_TYPE]))
