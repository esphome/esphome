import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_COLUMNS
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREE,
    CONF_NAME,
    CONF_TIME_ID,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_BYTES,
    UNIT_SECOND,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_TSDB_ID, DOMAIN, TsdbComponent

DEPENDENCIES = ["tsdb"]

CONF_AVERAGE = "average"
CONF_COLUMN = "column"
CONF_ERRORS = "errors"
CONF_EVICTIONS = "evictions"
CONF_MAX = "max"
CONF_MIN = "min"
CONF_NEWEST_AGE = "newest_age"
CONF_OLDEST_AGE = "oldest_age"
CONF_RECORDS = "records"
CONF_USED = "used"

_AGGREGATES = (CONF_AVERAGE, CONF_MIN, CONF_MAX)

_COUNTER_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    state_class=STATE_CLASS_TOTAL_INCREASING,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)
_BYTES_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    unit_of_measurement=UNIT_BYTES,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)
_AGE_SCHEMA = sensor.sensor_schema(
    accuracy_decimals=0,
    unit_of_measurement=UNIT_SECOND,
    device_class=DEVICE_CLASS_DURATION,
    entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
)

COLUMN_SENSORS_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_COLUMN): cv.string_strict,
        cv.Optional(CONF_AVERAGE): sensor.sensor_schema(),
        cv.Optional(CONF_MIN): sensor.sensor_schema(),
        cv.Optional(CONF_MAX): sensor.sensor_schema(),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TSDB_ID): cv.use_id(TsdbComponent),
        cv.Optional(CONF_RECORDS): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_USED): _BYTES_SCHEMA,
        cv.Optional(CONF_FREE): _BYTES_SCHEMA,
        cv.Optional(CONF_OLDEST_AGE): _AGE_SCHEMA,
        cv.Optional(CONF_NEWEST_AGE): _AGE_SCHEMA,
        cv.Optional(CONF_EVICTIONS): _COUNTER_SCHEMA,
        cv.Optional(CONF_ERRORS): _COUNTER_SCHEMA,
        cv.Optional(CONF_COLUMNS): cv.ensure_list(COLUMN_SENSORS_SCHEMA),
    }
)


def _column_names(hub: ConfigType) -> list[str]:
    return [column[CONF_NAME] for column in hub[CONF_COLUMNS]]


def _final_validate(config: ConfigType) -> None:
    columns = config.get(CONF_COLUMNS, [])
    if not columns:
        return
    hub = fv.full_config.get()[DOMAIN]
    names = _column_names(hub)
    for index, column in enumerate(columns):
        if column[CONF_COLUMN] not in names:
            raise cv.Invalid(
                f"'{column[CONF_COLUMN]}' is not a tsdb column (have: {', '.join(names)})",
                path=[CONF_COLUMNS, index, CONF_COLUMN],
            )
    # Aggregates are queried over a time window, so without a clock they stay unknown.
    if CONF_TIME_ID not in hub and any(
        key in column for column in columns for key in _AGGREGATES
    ):
        raise cv.Invalid(
            "average, min and max need a time source; set time_id on tsdb",
            path=[CONF_COLUMNS],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TSDB_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RECORDS, hub.set_records_sensor)
    await sensors(CONF_USED, hub.set_used_sensor)
    await sensors(CONF_FREE, hub.set_free_sensor)
    await sensors(CONF_OLDEST_AGE, hub.set_oldest_age_sensor)
    await sensors(CONF_NEWEST_AGE, hub.set_newest_age_sensor)
    await sensors(CONF_EVICTIONS, hub.set_evictions_sensor)
    await sensors(CONF_ERRORS, hub.set_errors_sensor)

    names = _column_names(CORE.config[DOMAIN])
    for column in config.get(CONF_COLUMNS, []):
        index = names.index(column[CONF_COLUMN])
        if (average := column.get(CONF_AVERAGE)) is not None:
            cg.add(
                hub.set_column_average_sensor(index, await sensor.new_sensor(average))
            )
        if (low := column.get(CONF_MIN)) is not None:
            cg.add(hub.set_column_min_sensor(index, await sensor.new_sensor(low)))
        if (high := column.get(CONF_MAX)) is not None:
            cg.add(hub.set_column_max_sensor(index, await sensor.new_sensor(high)))
