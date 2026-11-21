"""Sensors exposed by the ssocr_reader component."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_VALUE
from esphome.types import ConfigType

from . import SSOCRReader

DEPENDENCIES = ["ssocr_reader"]
CONF_SSOCR_READER_ID = "ssocr_reader_id"
CONF_CONFIDENCE = "confidence"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SSOCR_READER_ID): cv.use_id(SSOCRReader),
        cv.Optional(CONF_VALUE): sensor.sensor_schema(),
        cv.Optional(CONF_CONFIDENCE): sensor.sensor_schema(
            unit_of_measurement="%", accuracy_decimals=1, icon="mdi:percent"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SSOCR_READER_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_VALUE, hub.set_value_sensor)
    await sensors(CONF_CONFIDENCE, hub.set_confidence_sensor)
