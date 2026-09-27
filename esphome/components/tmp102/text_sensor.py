"""TMP102 threshold write status entity."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_TMP102_ID, TMP102Component, validate_unique_child

DEPENDENCIES = ["sensor"]

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.Required(CONF_TMP102_ID): cv.use_id(TMP102Component),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    return validate_unique_child(config, "text_sensor")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_TEXT_SENSOR")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(parent.set_threshold_status_text_sensor(var))
    cg.add(parent.set_configure(True))
