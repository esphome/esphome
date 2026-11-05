from dataclasses import dataclass

from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ICON_THERMOMETER
from esphome.types import ConfigType

from .. import (
    CHARGER_STATUS_1,
    CHARGER_STATUS_2,
    CONF_BQ25820_ID,
    DUMP,
    DUMP_TAG,
    STATUS,
    STATUS_PUBLISHER,
    BQ25820BaseSchema,
    bq25820_ns,
)

DEPENDENCIES = ["bq25820"]
_TEXT_SENSOR_TYPE = cg.RawExpression('LOG_STR_LITERAL("Text Sensor")')

ICON_BATTERY_CHARGING = "mdi:battery-charging"
_STRINGS = bq25820_ns.namespace("BQ25820StatusStrings")
_UNKNOWN = 5  # undefined TS codes 5-7 fall back to "Unknown"


@dataclass(frozen=True, kw_only=True)
class _StatusText:
    """The three-bit field (status[register] >> shift), named by BQ25820StatusStrings[first + field]."""

    key: str
    register: int
    shift: int = 0
    first: int = 0
    icon: str


_FIELDS = (
    _StatusText(
        key="charge_status", register=CHARGER_STATUS_1, icon=ICON_BATTERY_CHARGING
    ),
    _StatusText(
        key="temp_status",
        register=CHARGER_STATUS_2,
        shift=4,
        first=8,
        icon=ICON_THERMOMETER,
    ),
)


CONFIG_SCHEMA = cv.All(
    BQ25820BaseSchema.extend(
        {
            cv.Optional(field.key): text_sensor.text_sensor_schema(icon=field.icon)
            for field in _FIELDS
        }
    ),
    cv.has_at_least_one_key(*(field.key for field in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = text_sensor.sub_text_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            text_sensor.add_table_on_change(
                STATUS_PUBLISHER,
                hub,
                var,
                _STRINGS,
                STATUS[field.register] >> field.shift,
                0x07,
                field.first,
                _UNKNOWN,
            )
            DUMP.add(
                hub,
                text_sensor.text_sensor_ns.log_text_sensor(
                    DUMP_TAG, "  ", _TEXT_SENSOR_TYPE, parent_ref(var)
                ),
            )
