"""TMP102 threshold status entity."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import CONF_TMP102_ID, TMP102Component, validate_unique_child

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
).extend(
    {
        cv.GenerateID(CONF_TMP102_ID): cv.use_id(TMP102Component),
    }
)


def _final_validate(config: ConfigType) -> None:
    validate_unique_child(config, "text_sensor")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_TEXT_SENSOR")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    var = await text_sensor.new_text_sensor(config)
    cg.add(parent.set_threshold_status_text_sensor(var))
