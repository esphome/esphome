"""Expose the RX8111's latched flags and live input/backup status."""

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_PROBLEM,
    ENTITY_CATEGORY_DIAGNOSTIC,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_RX8111_ID
from .time import RX8111Component

DEPENDENCIES = ["time"]

# Order matches the parent's status sensor slots; the first five use FLAG.
STATUS_SENSORS = {
    "vlf": DEVICE_CLASS_PROBLEM,
    "alarm_flag": None,
    "timer_flag": None,
    "xst": DEVICE_CLASS_PROBLEM,
    "event_flag": None,
    "battery_low": DEVICE_CLASS_BATTERY,
    "evin": None,
}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_RX8111_ID): cv.use_id(RX8111Component),
            **{
                cv.Optional(key): binary_sensor.binary_sensor_schema(
                    device_class=device_class
                    if device_class is not None
                    else cv.UNDEFINED,
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
                )
                for key, device_class in STATUS_SENSORS.items()
            },
        }
    ),
    cv.has_at_least_one_key(*STATUS_SENSORS),
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain="binary_sensor",
    platform="rx8111",
    keys=(CONF_RX8111_ID,),
    message="Only one binary sensor platform block is allowed per RX8111",
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_RX8111_ID])
    for index, key in enumerate(STATUS_SENSORS):
        if (sensor_config := config.get(key)) is not None:
            sensor = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(parent.set_status_sensor(index, sensor))
