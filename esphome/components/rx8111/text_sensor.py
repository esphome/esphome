"""Publish timestamps captured by the RX8111 EVIN input."""

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_TIMESTAMP
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_RX8111_ID
from .time import RX8111Component, rx8111_ns

DEPENDENCIES = ["time"]

CONF_TIMESTAMP_RECORD_MODE = "timestamp_record_mode"
TimestampMode = rx8111_ns.enum("TimestampMode")
TIMESTAMP_MODES = {
    "latest": TimestampMode.TIMESTAMP_MODE_LATEST,
    "stop_when_full": TimestampMode.TIMESTAMP_MODE_STOP_WHEN_FULL,
    "overwrite": TimestampMode.TIMESTAMP_MODE_OVERWRITE,
}

CONFIG_SCHEMA = text_sensor.text_sensor_schema(
    device_class=DEVICE_CLASS_TIMESTAMP
).extend(
    {
        cv.GenerateID(CONF_RX8111_ID): cv.use_id(RX8111Component),
        cv.Optional(CONF_TIMESTAMP_RECORD_MODE, default="latest"): cv.enum(
            TIMESTAMP_MODES, lower=True
        ),
    }
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="text_sensor",
    platform="rx8111",
    keys=(CONF_RX8111_ID,),
    message="Only one timestamp text sensor is allowed per RX8111",
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_RX8111_TIMESTAMP")
    parent = await cg.get_variable(config[CONF_RX8111_ID])
    sensor = await text_sensor.new_text_sensor(config)
    cg.add(
        parent.set_event_timestamp_sensor(sensor, config[CONF_TIMESTAMP_RECORD_MODE])
    )
