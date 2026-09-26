"""``tsdb`` - ESPHome data-logging hub on top of the esp_tsdb engine.

Appends one row per ``update_interval`` to a time-series database on a LittleFS
partition, so the board keeps a history of its own readings while Home Assistant
is unreachable (up to the newest ``sync_interval`` of data is at risk on a power
cut - see ``components/tsdb/README.md`` and ``docs/data_logging.md``).

Two third-party engines are pulled from the ESP-IDF component registry, both
pinned by version and both MIT licensed:

* ``zakery292/esp_tsdb`` - the data-agnostic time-series database (columnar
  storage, sparse index, ring-buffer eviction, aggregations).
* ``joltwallet/littlefs`` - LittleFS, the fail-safe filesystem the database
  lives on.

The component asks ESPHome for its flash partition (``esp32.add_partition()``),
so a 512 KB partition costs each of the two OTA slots 256 KB - a full flash over
USB is required once after the partition table changes.
"""

import re

import esphome.codegen as cg
from esphome.components import sensor, text_sensor
from esphome.components.const import CONF_COLUMNS
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    add_partition,
    require_vfs_dir,
)
from esphome.components.time import RealTimeClock
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_NAME,
    CONF_OFFSET,
    CONF_SENSOR,
    CONF_TIME_ID,
    CONF_UPDATE_INTERVAL,
    Framework,
    PlatformFramework,
)
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "text_sensor"]

tsdb_ns = cg.esphome_ns.namespace("tsdb")
TsdbComponent = tsdb_ns.class_("TsdbComponent", cg.PollingComponent)

#: Registry coordinates of the two engines, pinned to the version this component
#: was written against (the file format of the database depends on it).
ESP_TSDB_COMPONENT = "zakery292/esp_tsdb"
ESP_TSDB_VERSION = "2.4.3"
ESP_LITTLEFS_COMPONENT = "joltwallet/littlefs"
ESP_LITTLEFS_VERSION = "1.22.3"

#: esp_tsdb stores <= 16 base parameters in the V3 file format (17..64 need the
#: V4 wide layout); a seventh column is a schema migration, not a config change.
TSDB_MAX_BASE_PARAMS = 16
#: `char param_names[16][32]` in the esp_tsdb file header.
TSDB_MAX_NAME_LENGTH = 31
#: LittleFS block size - partition sizes have to be a multiple of it.
LITTLEFS_BLOCK_BYTES = 0x1000

CONF_MOUNT_POINT = "mount_point"
CONF_PARTITION = "partition"
CONF_PARTITION_SIZE = "partition_size"
CONF_FORMAT_ON_FIRST_BOOT = "format_on_first_boot"
CONF_RECREATE_ON_SCHEMA_CHANGE = "recreate_on_schema_change"
CONF_MAX_FILE_SIZE = "max_file_size"
CONF_INDEX_STRIDE = "index_stride"
CONF_BUFFER_POOL_SIZE = "buffer_pool_size"
CONF_MEMORY = "memory"
CONF_PAGED_ALLOCATION = "paged_allocation"
CONF_PAGE_SIZE = "page_size"
CONF_SYNC_INTERVAL = "sync_interval"
CONF_MIN_FREE_BYTES = "min_free_bytes"
CONF_REQUIRE_TIME = "require_time"
CONF_ON_MISSING = "on_missing"
#: Not `scale`: `CONF_SCALE` is defined by the `lvgl` and `qr_code` components, and a
#: third definition fails `lint_constants_usage` (see `ci-custom.py`).
CONF_SCALE_FACTOR = "scale_factor"
CONF_AVERAGE_SENSOR = "average_sensor"
CONF_MIN_SENSOR = "min_sensor"
CONF_MAX_SENSOR = "max_sensor"
CONF_AGGREGATE_WINDOW = "aggregate_window"
CONF_AGGREGATE_INTERVAL = "aggregate_interval"
CONF_DUMP_ROWS = "dump_rows"
CONF_RECORDS_SENSOR = "records_sensor"
CONF_USED_SENSOR = "used_sensor"
CONF_FREE_SENSOR = "free_sensor"
CONF_OLDEST_SENSOR = "oldest_sensor"
CONF_NEWEST_SENSOR = "newest_sensor"
CONF_ERRORS_SENSOR = "errors_sensor"
#: Mirrors the log lines of the database into a text sensor.
CONF_LOG_SENSOR = "log_sensor"

# Buffer pool location, mirrors tsdb_alloc_strategy_t (esp_tsdb include/esp_tsdb.h).
MEMORY_MODES = {
    "internal": 0,  # TSDB_ALLOC_INTERNAL_RAM
    "psram": 1,  # TSDB_ALLOC_PSRAM
    "auto": 2,  # TSDB_ALLOC_AUTO
}

# A column without a fresh value, mirrors tsdbmath::MissingPolicy (tsdb_math.h).
MISSING_POLICIES = {
    "skip": 0,  # drop the whole record (default)
    "hold": 1,  # repeat the last value written for that column
    "sentinel": 2,  # write -32768 for that column
}

_SIZE_UNITS = {
    "": 1,
    "b": 1,
    "kb": 1024,
    "kib": 1024,
    "mb": 1024 * 1024,
    "mib": 1024 * 1024,
}
_SIZE_PATTERN = re.compile(r"^([0-9]+(?:\.[0-9]+)?)\s*([A-Za-z]*)$")


def _validate_size(value: object, *, allow_zero: bool = False) -> int:
    """Accept ``512KB``, ``2MB``, ``0x40000`` or a plain byte count and return bytes."""
    if isinstance(value, bool):
        raise cv.Invalid(f"'{value}' is not a size (use e.g. 512KB, 2MB or 524288)")
    if isinstance(value, int):
        bytes_ = value
    else:
        text = str(value).strip()
        if text.lower().startswith("0x"):
            try:
                bytes_ = int(text, 16)
            except ValueError as err:
                raise cv.Invalid(
                    f"'{value}' is not a valid size (use e.g. 512KB, 2MB or 524288)"
                ) from err
        else:
            match = _SIZE_PATTERN.match(text)
            factor = _SIZE_UNITS.get(match.group(2).lower()) if match else None
            if match is None:
                raise cv.Invalid(
                    f"'{value}' is not a valid size (use e.g. 512KB, 2MB or 524288)"
                )
            if factor is None:
                raise cv.Invalid(
                    f"unknown unit '{match.group(2)}' in '{value}' (use B, KB or MB)"
                )
            bytes_ = int(float(match.group(1)) * factor)
    if bytes_ < 0 or (bytes_ == 0 and not allow_zero):
        raise cv.Invalid(f"'{value}' must be a positive size")
    return bytes_


def _validate_partition_size(value: object) -> int:
    """A LittleFS partition is a whole number of 4 KB blocks."""
    bytes_ = _validate_size(value)
    if bytes_ % LITTLEFS_BLOCK_BYTES != 0:
        raise cv.Invalid(
            f"partition_size must be a multiple of 4KB, '{value}' is not (use e.g. 512KB)"
        )
    if bytes_ < 16 * LITTLEFS_BLOCK_BYTES:
        raise cv.Invalid(f"partition_size must be at least 64KB, got '{value}'")
    return bytes_


def _validate_mount_point(value: str) -> str:
    if not value.startswith("/") or value.endswith("/"):
        raise cv.Invalid(
            f"mount_point must be an absolute path without a trailing slash, got '{value}'"
        )
    return value


def _validate_file_name(value: str) -> str:
    if "/" in value or "\\" in value:
        raise cv.Invalid(
            f"file must be a plain file name inside mount_point, got '{value}'"
        )
    return value


def _validate_memory(value: str) -> str:
    mode = cv.one_of(*MEMORY_MODES, lower=True)(value)
    if mode == "psram":
        # PSRAM must exist and be guaranteed, otherwise the database would fall
        # back silently; `psram:` also has to be configured (see psram component).
        return cv.requires_component("psram")(mode)
    return mode


def _validate_min_free_bytes(value: object) -> int:
    """Like `_validate_size`, but `0` disables the free-space guard."""
    return _validate_size(value, allow_zero=True)


def _validate_config(config: ConfigType) -> ConfigType:
    """Everything that only fails once the whole block is visible."""
    columns = config[CONF_COLUMNS]
    names = [column[CONF_NAME] for column in columns]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise cv.Invalid(
            f"duplicate column name(s): {', '.join(duplicates)} - every column needs its own name"
        )
    for column in columns:
        if column[CONF_SCALE_FACTOR] == 0:
            raise cv.Invalid(
                f"'{column[CONF_NAME]}': scale_factor must not be 0 - without it every reading would be stored as -32768"
            )

    if config[CONF_REQUIRE_TIME] and CONF_TIME_ID not in config:
        raise cv.Invalid(
            "`require_time: true` needs a `time_id:` (a `time:`/`sntp:` component) - or set `require_time: false` "
            "to accept timestamps from a clock that may not be set yet"
        )

    partition = config[CONF_PARTITION_SIZE]
    min_free = config[CONF_MIN_FREE_BYTES]
    max_file = config[CONF_MAX_FILE_SIZE]
    if min_free >= partition:
        raise cv.Invalid(
            f"min_free_bytes ({min_free} bytes) must stay below partition_size ({partition} bytes) - "
            "LittleFS needs free blocks to write at all"
        )
    if max_file + min_free > partition:
        raise cv.Invalid(
            f"max_file_size + min_free_bytes = {max_file + min_free} bytes do not fit into partition_size "
            f"({partition} bytes): raise partition_size or lower max_file_size"
        )
    return config


COLUMN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_NAME): cv.All(
            cv.string_strict,
            # char param_names[16][32] in the esp_tsdb file header
            cv.Length(min=1, max=TSDB_MAX_NAME_LENGTH),
        ),
        cv.Optional(CONF_SCALE_FACTOR, default=1.0): cv.float_,
        cv.Optional(CONF_OFFSET, default=0.0): cv.float_,
        cv.Optional(CONF_AVERAGE_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_MIN_SENSOR): cv.use_id(sensor.Sensor),
        cv.Optional(CONF_MAX_SENSOR): cv.use_id(sensor.Sensor),
    }
)

CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    # Both engines are ESP-IDF components: only the ESP-IDF framework can build them.
    cv.only_with_framework(Framework.ESP_IDF),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TsdbComponent),
            cv.Optional(CONF_FILE, default="history.tsdb"): cv.All(
                cv.string_strict, cv.Length(min=1, max=64), _validate_file_name
            ),
            cv.Optional(CONF_MOUNT_POINT, default="/littlefs"): cv.All(
                cv.string_strict, cv.Length(min=2, max=32), _validate_mount_point
            ),
            cv.Optional(CONF_PARTITION, default="littlefs"): cv.All(
                cv.string_strict, cv.Length(min=1, max=15)
            ),
            cv.Optional(CONF_PARTITION_SIZE, default="512KB"): _validate_partition_size,
            cv.Optional(CONF_FORMAT_ON_FIRST_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_RECREATE_ON_SCHEMA_CHANGE, default=False): cv.boolean,
            cv.Optional(CONF_MAX_FILE_SIZE, default="384KB"): _validate_size,
            cv.Optional(CONF_INDEX_STRIDE, default=380): cv.int_range(
                min=1, max=100000
            ),
            cv.Optional(CONF_BUFFER_POOL_SIZE, default="4KB"): cv.All(
                _validate_size, cv.Range(min=1024, max=256 * 1024)
            ),
            cv.Optional(CONF_MEMORY, default="auto"): _validate_memory,
            cv.Optional(CONF_PAGED_ALLOCATION, default=False): cv.boolean,
            cv.Optional(CONF_PAGE_SIZE, default="2KB"): cv.All(
                _validate_size, cv.Range(min=1024, max=64 * 1024)
            ),
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
            # `0s` syncs after every write (most durable, most flash commits)
            cv.Optional(
                CONF_SYNC_INTERVAL, default="60s"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_FREE_BYTES, default="32KB"): _validate_min_free_bytes,
            cv.Optional(CONF_REQUIRE_TIME, default=True): cv.boolean,
            cv.Optional(CONF_TIME_ID): cv.use_id(RealTimeClock),
            cv.Optional(CONF_ON_MISSING, default="skip"): cv.one_of(
                *MISSING_POLICIES, lower=True
            ),
            cv.Optional(
                CONF_AGGREGATE_WINDOW, default="1h"
            ): cv.positive_time_period_seconds,
            cv.Optional(
                CONF_AGGREGATE_INTERVAL, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DUMP_ROWS, default=60): cv.int_range(min=1, max=1000),
            cv.Required(CONF_COLUMNS): cv.All(
                cv.ensure_list(COLUMN_SCHEMA),
                cv.Length(min=1, max=TSDB_MAX_BASE_PARAMS),
            ),
            cv.Optional(CONF_RECORDS_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_USED_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_FREE_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_OLDEST_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_NEWEST_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_ERRORS_SENSOR): cv.use_id(sensor.Sensor),
            cv.Optional(CONF_LOG_SENSOR): cv.use_id(text_sensor.TextSensor),
        }
    ),
    _validate_config,
)


async def to_code(config: ConfigType) -> None:
    # Both engines come from the ESP-IDF component registry and stay pinned: the
    # on-disk format of the database belongs to the esp_tsdb version.
    add_idf_component(name=ESP_TSDB_COMPONENT, ref=ESP_TSDB_VERSION)
    add_idf_component(name=ESP_LITTLEFS_COMPONENT, ref=ESP_LITTLEFS_VERSION)
    # ESPHome appends the partition and splits its size between app0 and app1.
    add_partition(
        config[CONF_PARTITION], "data", "littlefs", config[CONF_PARTITION_SIZE]
    )
    # esp_tsdb stat()s the database file; ESPHome disables VFS directory support
    # by default to save flash.
    require_vfs_dir()
    # LittleFS erases in bursts and this project runs the task watchdog at 30 s;
    # let the driver feed it during long flash operations.
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_WDT_RESET", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_file(config[CONF_FILE]))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_partition_label(config[CONF_PARTITION]))
    cg.add(var.set_partition_size(config[CONF_PARTITION_SIZE]))
    cg.add(var.set_format_on_first_boot(config[CONF_FORMAT_ON_FIRST_BOOT]))
    cg.add(var.set_recreate_on_schema_change(config[CONF_RECREATE_ON_SCHEMA_CHANGE]))
    cg.add(var.set_max_file_size(config[CONF_MAX_FILE_SIZE]))
    cg.add(var.set_index_stride(config[CONF_INDEX_STRIDE]))
    cg.add(var.set_buffer_pool_size(config[CONF_BUFFER_POOL_SIZE]))
    cg.add(var.set_memory_mode(MEMORY_MODES[config[CONF_MEMORY]]))
    cg.add(var.set_paged_allocation(config[CONF_PAGED_ALLOCATION]))
    cg.add(var.set_page_size(config[CONF_PAGE_SIZE]))
    cg.add(var.set_sync_interval(config[CONF_SYNC_INTERVAL]))
    cg.add(var.set_min_free_bytes(config[CONF_MIN_FREE_BYTES]))
    cg.add(var.set_require_time(config[CONF_REQUIRE_TIME]))
    cg.add(var.set_missing_policy(MISSING_POLICIES[config[CONF_ON_MISSING]]))
    cg.add(var.set_aggregate_window(config[CONF_AGGREGATE_WINDOW]))
    cg.add(var.set_aggregate_interval(config[CONF_AGGREGATE_INTERVAL]))
    cg.add(var.set_dump_rows(config[CONF_DUMP_ROWS]))
    if CONF_TIME_ID in config:
        cg.add(var.set_time_source(await cg.get_variable(config[CONF_TIME_ID])))

    for index, column in enumerate(config[CONF_COLUMNS]):
        cg.add(
            var.add_column(
                column[CONF_NAME], column[CONF_SCALE_FACTOR], column[CONF_OFFSET]
            )
        )
        cg.add(var.set_column_source(index, await cg.get_variable(column[CONF_SENSOR])))
        if (target := column.get(CONF_AVERAGE_SENSOR)) is not None:
            cg.add(var.set_column_average_sensor(index, await cg.get_variable(target)))
        if (target := column.get(CONF_MIN_SENSOR)) is not None:
            cg.add(var.set_column_min_sensor(index, await cg.get_variable(target)))
        if (target := column.get(CONF_MAX_SENSOR)) is not None:
            cg.add(var.set_column_max_sensor(index, await cg.get_variable(target)))

    for key, setter in (
        (CONF_RECORDS_SENSOR, var.set_records_sensor),
        (CONF_USED_SENSOR, var.set_used_sensor),
        (CONF_FREE_SENSOR, var.set_free_sensor),
        (CONF_OLDEST_SENSOR, var.set_oldest_sensor),
        (CONF_NEWEST_SENSOR, var.set_newest_sensor),
        (CONF_ERRORS_SENSOR, var.set_errors_sensor),
        (CONF_LOG_SENSOR, var.set_log_sensor),
    ):
        if (target := config.get(key)) is not None:
            cg.add(setter(await cg.get_variable(target)))


# `tsdb.cpp` drives the ESP-IDF only engine (esp_tsdb + littlefs) and cannot
# build for the host, where the C++ unit tests run: they cover the engine-free
# math of `tsdb_math.h`.
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "tsdb.cpp": {PlatformFramework.ESP32_IDF},
    }
)
