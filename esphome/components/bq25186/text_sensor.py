import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import (
    BQ25186_CHILD_SCHEMA,
    CONF_BQ25186_ID,
    STATUS,
    STATUS_PUBLISHER,
    bq25186_ns,
)

DEPENDENCIES = ["bq25186"]

REG_STAT0 = 0
REG_STAT1 = 1
_STRINGS = bq25186_ns.namespace("BQ25186StatusStrings")

# (key, status register, bit shift, index of the field's value 0 in the C++ string table)
_FIELDS = (
    ("charge_status", REG_STAT0, 5, 0),
    ("ts_status", REG_STAT1, 3, 4),
)


CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {cv.Optional(key): text_sensor.text_sensor_schema() for key, *_ in _FIELDS}
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for key, reg, shift, first in _FIELDS:
        if (var := await sensors(key, None)) is not None:
            text_sensor.add_table_on_change(
                STATUS_PUBLISHER, hub, var, _STRINGS, STATUS[reg] >> shift, 0x03, first
            )
