from dataclasses import dataclass

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


@dataclass(frozen=True, kw_only=True)
class _StatusText(cg.RegisterField):
    """The field named by BQ25186StatusStrings[first + field]."""

    key: str
    first: int = 0


_FIELDS = (
    _StatusText(key="charge_status", register=REG_STAT0, shift=5, mask=0x03),
    _StatusText(key="ts_status", register=REG_STAT1, shift=3, mask=0x03, first=4),
)


CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {cv.Optional(field.key): text_sensor.text_sensor_schema() for field in _FIELDS}
    ),
    cv.has_at_least_one_key(*(field.key for field in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            text_sensor.add_table_on_change(
                STATUS_PUBLISHER,
                hub,
                var,
                STATUS,
                field,
                table=_STRINGS,
                first=field.first,
            )
