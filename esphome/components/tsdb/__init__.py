"""tsdb - ESPHome data-logging hub on the esp_tsdb engine and LittleFS."""

import math
import re

from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
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
DOMAIN = "tsdb"
AUTO_LOAD = ["sensor"]

tsdb_ns = cg.esphome_ns.namespace("tsdb")
TsdbComponent = tsdb_ns.class_("TsdbComponent", cg.PollingComponent)

tsdbmath_ns = tsdb_ns.namespace("tsdbmath")
MissingPolicy = tsdbmath_ns.enum("MissingPolicy")
MemoryMode = tsdbmath_ns.enum("MemoryMode")

ESP_TSDB_COMPONENT = "zakery292/esp_tsdb"
ESP_TSDB_VERSION = "2.4.3"
ESP_LITTLEFS_COMPONENT = "joltwallet/littlefs"
ESP_LITTLEFS_VERSION = "1.22.3"

# Managed IDF components tsdb's C++ needs; to_code() adds them for a configured
# build, the clang-tidy project reads this attribute instead (see clang_tidy.py).
IDF_COMPONENTS = {
    ESP_TSDB_COMPONENT: ESP_TSDB_VERSION,
    ESP_LITTLEFS_COMPONENT: ESP_LITTLEFS_VERSION,
}

TSDB_MAX_BASE_PARAMS = 16
TSDB_MAX_NAME_LENGTH = 31
# Mirrors tsdbmath::INDEX_DEFAULT_STRIDE; keep the two in step.
TSDB_DEFAULT_INDEX_STRIDE = 380
LITTLEFS_BLOCK_BYTES = 0x1000
# LittleFS keeps its own metadata, so a budget that fills the partition cannot be written to.
LITTLEFS_RESERVED_BYTES = 4 * LITTLEFS_BLOCK_BYTES

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
CONF_SCALE_FACTOR = "scale_factor"
CONF_AGGREGATE_WINDOW = "aggregate_window"
CONF_AGGREGATE_INTERVAL = "aggregate_interval"
CONF_DUMP_ROWS = "dump_rows"
CONF_TSDB_ID = "tsdb_id"

MEMORY_MODES = {
    "internal": MemoryMode.MEMORY_INTERNAL,
    "psram": MemoryMode.MEMORY_PSRAM,
    "auto": MemoryMode.MEMORY_AUTO,
}

MISSING_POLICIES = {
    "skip": MissingPolicy.MISSING_SKIP,
    "hold": MissingPolicy.MISSING_HOLD,
    "sentinel": MissingPolicy.MISSING_SENTINEL,
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
    """Accept 512KB, 2MB, 0x40000 or a plain byte count."""
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
            scaled = float(match.group(1)) * factor
            if not scaled.is_integer():
                raise cv.Invalid(
                    f"'{value}' does not resolve to a whole number of bytes"
                )
            bytes_ = int(scaled)
    if bytes_ < 0 or (bytes_ == 0 and not allow_zero):
        raise cv.Invalid(f"'{value}' must be a positive size")
    return bytes_


def _validate_partition_size(value: object) -> int:
    bytes_ = _validate_size(value)
    if bytes_ % LITTLEFS_BLOCK_BYTES != 0:
        raise cv.Invalid(f"partition_size must be a multiple of 4KB, '{value}' is not")
    if bytes_ < 16 * LITTLEFS_BLOCK_BYTES:
        raise cv.Invalid(f"partition_size must be at least 64KB, got '{value}'")
    return bytes_


def _validate_mount_point(value: str) -> str:
    if not value.startswith("/") or value.endswith("/"):
        raise cv.Invalid(
            f"mount_point must be absolute without trailing slash, got '{value}'"
        )
    return value


def _validate_file_name(value: str) -> str:
    if "/" in value or "\\" in value:
        raise cv.Invalid(f"file must be a plain file name, got '{value}'")
    return value


def _validate_min_free_bytes(value: object) -> int:
    return _validate_size(value, allow_zero=True)


def _validate_memory(value: str) -> str:
    mode = cv.one_of(*MEMORY_MODES, lower=True)(value)
    if mode == "psram":
        return cv.requires_component("psram")(mode)
    return mode


def _validate_config(config: ConfigType) -> ConfigType:
    columns = config[CONF_COLUMNS]
    names = [column[CONF_NAME] for column in columns]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise cv.Invalid(f"duplicate column name(s): {', '.join(duplicates)}")
    for column in columns:
        name = column[CONF_NAME]
        scale = column[CONF_SCALE_FACTOR]
        # YAML `.nan`/`.inf` pass cv.float_ and would reach the int16 conversion
        # (undefined behaviour) or be emitted as the invalid C++ literal `inff`.
        if not math.isfinite(scale):
            raise cv.Invalid(f"'{name}': scale_factor must be a finite number")
        if scale == 0:
            raise cv.Invalid(f"'{name}': scale_factor must not be 0")
        if not math.isfinite(column[CONF_OFFSET]):
            raise cv.Invalid(f"'{name}': offset must be a finite number")
    partition = config[CONF_PARTITION_SIZE]
    min_free = config[CONF_MIN_FREE_BYTES]
    max_file = config[CONF_MAX_FILE_SIZE]
    if min_free >= partition:
        raise cv.Invalid("min_free_bytes must stay below partition_size")
    if max_file + min_free > partition:
        raise cv.Invalid(
            "max_file_size + min_free_bytes do not fit into partition_size"
        )
    if max_file + min_free + LITTLEFS_RESERVED_BYTES > partition:
        raise cv.Invalid(
            "max_file_size + min_free_bytes leave no room for the filesystem "
            f"metadata in partition_size (at least {LITTLEFS_RESERVED_BYTES // 1024}KB "
            "is needed)"
        )
    return config


COLUMN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_NAME): cv.All(
            cv.string_strict,
            cv.Length(min=1, max=TSDB_MAX_NAME_LENGTH),
        ),
        cv.Optional(CONF_SCALE_FACTOR, default=1.0): cv.float_,
        cv.Optional(CONF_OFFSET, default=0.0): cv.float_,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
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
            cv.Optional(
                CONF_INDEX_STRIDE, default=TSDB_DEFAULT_INDEX_STRIDE
            ): cv.int_range(min=1, max=100000),
            cv.Optional(CONF_BUFFER_POOL_SIZE, default="4KB"): cv.All(
                _validate_size, cv.Range(min=1024, max=256 * 1024)
            ),
            cv.Optional(CONF_MEMORY, default="auto"): _validate_memory,
            cv.Optional(CONF_PAGED_ALLOCATION, default=False): cv.boolean,
            cv.Optional(CONF_PAGE_SIZE, default="2KB"): cv.All(
                _validate_size, cv.Range(min=1024, max=64 * 1024)
            ),
            cv.Optional(CONF_UPDATE_INTERVAL, default="60s"): cv.update_interval,
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
        }
    ),
    _validate_config,
)

# Actions target the hub by id: `tsdb.flush: tsdb1`.
TSDB_ACTION_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(TsdbComponent)})

automation.register_apply_action(
    "tsdb.flush", TSDB_ACTION_SCHEMA, automation.ApplyCall("request_flush()")
)
automation.register_apply_action(
    "tsdb.dump", TSDB_ACTION_SCHEMA, automation.ApplyCall("request_dump()")
)
automation.register_apply_action(
    "tsdb.clear", TSDB_ACTION_SCHEMA, automation.ApplyCall("request_clear()")
)


async def to_code(config: ConfigType) -> None:
    for name, ref in IDF_COMPONENTS.items():
        add_idf_component(name=name, ref=ref)
    add_partition(
        config[CONF_PARTITION], "data", "littlefs", config[CONF_PARTITION_SIZE]
    )
    require_vfs_dir()
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

    cg.add_define("TSDB_COLUMN_COUNT", len(config[CONF_COLUMNS]))
    for index, column in enumerate(config[CONF_COLUMNS]):
        cg.add(
            var.add_column(
                column[CONF_NAME], column[CONF_SCALE_FACTOR], column[CONF_OFFSET]
            )
        )
        cg.add(var.set_column_source(index, await cg.get_variable(column[CONF_SENSOR])))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "tsdb.cpp": {PlatformFramework.ESP32_IDF},
    }
)
