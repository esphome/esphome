import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_COLUMNS
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_FREE,
    CONF_NAME,
    CONF_SENSOR,
    CONF_TIME_ID,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_BYTES,
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
# Metadata an aggregate takes from the sensor it summarizes.
_METADATA = (CONF_UNIT_OF_MEASUREMENT, CONF_DEVICE_CLASS)

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
    kind=sensor.DURATION_S,
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


def _inherit_source_metadata(
    hub: ConfigType, columns: list[ConfigType], full: Config
) -> None:
    """Give each aggregate the unit and device class of the sensor it summarizes.

    Units and device classes are registered during code generation, so this has to be
    decided here, before the sensors are built. A value set on the aggregate wins,
    an empty one included.
    """
    by_name = {column[CONF_NAME]: column for column in hub[CONF_COLUMNS]}
    for column in columns:
        # Looked up by ID, so a sensor nested in a platform (a DHT's humidity) is found.
        path = full.get_path_for_id(by_name[column[CONF_COLUMN]][CONF_SENSOR])
        source = full.get_config_for_path(path[:-1])
        for key in _AGGREGATES:
            aggregate = column.get(key)
            if aggregate is None:
                continue
            for metadata in _METADATA:
                if metadata not in aggregate and (value := source.get(metadata)):
                    aggregate[metadata] = value


def _final_validate(config: ConfigType) -> None:
    columns = config.get(CONF_COLUMNS, [])
    if not columns:
        return
    full = fv.full_config.get()
    hub = full[DOMAIN]
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
    _inherit_source_metadata(hub, columns, full)


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
