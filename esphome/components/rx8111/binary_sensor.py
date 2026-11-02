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
from .time import RX8111Component, rx8111_ns

DEPENDENCIES = ["time"]

STATUS_SENSORS: dict[str, str | None] = {
    "vlf": DEVICE_CLASS_PROBLEM,
    "alarm_flag": None,
    "timer_flag": None,
    "xst": DEVICE_CLASS_PROBLEM,
    "event_flag": None,
    "battery_low": DEVICE_CLASS_BATTERY,
    "evin": None,
}
StatusSensor = rx8111_ns.enum("StatusSensor")
STATUS_SENSOR_SLOTS = {
    "vlf": StatusSensor.STATUS_SENSOR_VLF,
    "alarm_flag": StatusSensor.STATUS_SENSOR_ALARM_FLAG,
    "timer_flag": StatusSensor.STATUS_SENSOR_TIMER_FLAG,
    "xst": StatusSensor.STATUS_SENSOR_XST,
    "event_flag": StatusSensor.STATUS_SENSOR_EVENT_FLAG,
    "battery_low": StatusSensor.STATUS_SENSOR_BATTERY_LOW,
    "evin": StatusSensor.STATUS_SENSOR_EVIN,
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
    for key, slot in STATUS_SENSOR_SLOTS.items():
        if (sensor_config := config.get(key)) is not None:
            sensor = await binary_sensor.new_binary_sensor(sensor_config)
            cg.add(parent.set_status_sensor(slot, sensor))
