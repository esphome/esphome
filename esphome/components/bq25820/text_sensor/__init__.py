from functools import partial

import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_THERMOMETER
from esphome.types import ConfigType

from .. import CONF_BQ25820_ID, BQ25820BaseSchema, bq25820_ns, register_text_sensor

DEPENDENCIES = ["bq25820"]

BQ25820TextSensor = bq25820_ns.class_("BQ25820TextSensor", text_sensor.TextSensor)

ICON_BATTERY_CHARGING = "mdi:battery-charging"

# (key, status register, bit shift, index of the field's value 0 in the C++ string table, icon)
_FIELDS = (
    ("charge_status", 0, 0, 0, ICON_BATTERY_CHARGING),
    ("temp_status", 1, 4, 8, ICON_THERMOMETER),
)

CONFIG_SCHEMA = BQ25820BaseSchema.extend(
    {
        cv.Optional(key): text_sensor.text_sensor_schema(BQ25820TextSensor, icon=icon)
        for key, *_, icon in _FIELDS
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for key, reg, shift, first, _ in _FIELDS:
        await sensors(key, partial(register_text_sensor, hub), reg, shift, first)
