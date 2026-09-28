"""TMP102 comparator alert entity."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_TMP102_ID, TMP102Component, validate_unique_child

DEPENDENCIES = ["sensor"]

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema().extend(
    {
        cv.Required(CONF_TMP102_ID): cv.use_id(TMP102Component),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    return validate_unique_child(config, "binary_sensor")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_TMP102_BINARY_SENSOR")
    parent = await cg.get_variable(config[CONF_TMP102_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(parent.set_alert_binary_sensor(var))
