"""tsdb - ESPHome data-logging hub on the esp_tsdb engine and LittleFS."""

from collections.abc import Callable
import math
import re

from esphome import automation
import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_COLUMNS
from esphome.components.esp32 import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    add_idf_sdkconfig_option,
    add_partition,
    only_on_variant,
    require_vfs_dir,
)
from esphome.components.time import RealTimeClock
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILE,
    CONF_ID,
    CONF_NAME,
    CONF_OFFSET,
    CONF_PLATFORM,
    CONF_SENSOR,
    CONF_TIME,
    CONF_TIME_ID,
    Framework,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

AUTO_LOAD = ["sensor"]
CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tsdb"

tsdb_ns = cg.esphome_ns.namespace("tsdb")
TsdbComponent = tsdb_ns.class_("TsdbComponent", cg.PollingComponent)

tsdbmath_ns = tsdb_ns.namespace("tsdbmath")
MissingPolicy = tsdbmath_ns.enum("MissingPolicy")
MemoryMode = tsdbmath_ns.enum("MemoryMode")

ESP_TSDB_COMPONENT = "zakery292/esp_tsdb"
ESP_TSDB_VERSION = "2.4.4"
ESP_LITTLEFS_COMPONENT = "joltwallet/littlefs"
ESP_LITTLEFS_VERSION = "1.22.3"

# Managed IDF components the C++ needs; esphome/idf_component.yml declares the same pins.
IDF_COMPONENTS = {
    ESP_TSDB_COMPONENT: ESP_TSDB_VERSION,
    ESP_LITTLEFS_COMPONENT: ESP_LITTLEFS_VERSION,
}

TSDB_MAX_BASE_PARAMS = 16
TSDB_MAX_NAME_LENGTH = 31
# Mirrors tsdbmath::INDEX_DEFAULT_STRIDE; keep the two in step.
TSDB_DEFAULT_INDEX_STRIDE = 380
# The engine's on-disk block size, its offset inside the buffer pool (read block,
# write cache, then the query block), and the in-memory block the pool hands out.
# Mirrors tsdbmath::BLOCK_SIZE and tsdbmath::BLOCK_IN_MEMORY_BYTES; keep in step.
TSDB_BLOCK_BYTES = 1024
TSDB_BLOCK_IN_MEMORY_BYTES = 1376
TSDB_QUERY_BLOCK_OFFSET = 2 * TSDB_BLOCK_BYTES
TSDB_POOL_BLOCKS = (0, TSDB_BLOCK_BYTES, TSDB_QUERY_BLOCK_OFFSET)
# Smallest page that holds every block of the pool inside one page.
TSDB_DEFAULT_PAGE_SIZE = 4 * TSDB_BLOCK_BYTES
LITTLEFS_BLOCK_BYTES = 0x1000
LITTLEFS_RESERVED_BYTES = 4 * LITTLEFS_BLOCK_BYTES
# The engine keeps those names in a 32-byte field, and tsdb.cpp builds paths in a
# 128-byte buffer: both limits are bytes, not characters.
FILE_NAME_MAX_BYTES = 64
MOUNT_POINT_MAX_BYTES = 32

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

MEMORY_MODES = MemoryMode.options("internal", "psram", "auto", prefix="MEMORY_")
MISSING_POLICIES = MissingPolicy.options("skip", "hold", "sentinel", prefix="MISSING_")

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


def _utf8_bytes(max_bytes: int) -> Callable[[str], str]:
    """Bound a string by its UTF-8 byte length, which is what a char buffer holds."""

    def validate(value: str) -> str:
        if (length := len(value.encode("utf-8"))) > max_bytes:
            raise cv.Invalid(
                f"must be at most {max_bytes} bytes when UTF-8 encoded, got {length}"
            )
        return value

    return validate


def _validate_column_name(value: str) -> str:
    # The name is written into the CSV header, so a separator or a line break would
    # split it into extra columns the records do not have.
    if any(character in value for character in ',"\n\r'):
        raise cv.Invalid(
            "column name must not contain a comma, a double quote or a line break"
        )
    return value


def _validate_min_free_bytes(value: object) -> int:
    return _validate_size(value, allow_zero=True)


def _validate_buffer_pool_size(value: object) -> int:
    bytes_ = _validate_size(value)
    # The engine slices the pool into a read block, a write cache and a query block, each
    # a full in-memory block, so a smaller pool leaves them outside it and every read,
    # write and query has to allocate one.
    minimum = TSDB_QUERY_BLOCK_OFFSET + TSDB_BLOCK_IN_MEMORY_BYTES
    if bytes_ <= minimum:
        raise cv.Invalid(
            f"buffer_pool_size must be larger than {minimum} bytes: the engine needs a "
            f"{TSDB_BLOCK_IN_MEMORY_BYTES}-byte read block, write cache and query block"
        )
    return bytes_


def _validate_page_geometry(config: ConfigType) -> None:
    if not config.get(CONF_PAGED_ALLOCATION, False):
        return
    page = config.get(CONF_PAGE_SIZE, TSDB_DEFAULT_PAGE_SIZE)
    # Every pool block has to stay inside one page, otherwise the engine allocates and
    # frees a fallback buffer for each read, write, aggregate and CSV query.
    for offset in TSDB_POOL_BLOCKS:
        if offset % page + TSDB_BLOCK_IN_MEMORY_BYTES > page:
            raise cv.Invalid(
                f"page_size {page} splits an engine block across pages: use a page of "
                f"{TSDB_DEFAULT_PAGE_SIZE} bytes or more, or turn paged_allocation off"
            )


def _require_psram_component(mode: str) -> str:
    if mode == "psram":
        return cv.requires_component("psram")(mode)
    return mode


# Composed, so the language schema dumper sees the choices.
_validate_memory = cv.All(
    cv.one_of(*MEMORY_MODES, lower=True), _require_psram_component
)


def _validate_config(config: ConfigType) -> ConfigType:
    columns = config[CONF_COLUMNS]
    names = [column[CONF_NAME] for column in columns]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise cv.Invalid(f"duplicate column name(s): {', '.join(duplicates)}")
    for column in columns:
        name = column[CONF_NAME]
        scale = column[CONF_SCALE_FACTOR]
        # YAML `.nan`/`.inf` pass cv.float_ and reach the int16 cast as UB, or as the literal `inff`.
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
    # Folding the header rewrites the whole file, and LittleFS is copy-on-write, so the
    # partition needs room for a second copy of the history at the same time, on top of
    # the free-space reserve and the filesystem metadata.
    needed = 2 * max_file + min_free + LITTLEFS_RESERVED_BYTES
    if needed > partition:
        raise cv.Invalid(
            "partition_size must hold a second copy of max_file_size while the header "
            f"folds: 2 * {max_file} + {min_free} (min_free_bytes) + "
            f"{LITTLEFS_RESERVED_BYTES} (filesystem metadata) needs {needed} bytes, "
            f"partition_size is {partition}"
        )
    _validate_page_geometry(config)
    return config


COLUMN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_SENSOR): cv.use_id(sensor.Sensor),
        cv.Required(CONF_NAME): cv.All(
            cv.string_strict,
            cv.Length(min=1, max=TSDB_MAX_NAME_LENGTH),
            _utf8_bytes(TSDB_MAX_NAME_LENGTH),
            _validate_column_name,
        ),
        cv.Optional(CONF_SCALE_FACTOR, default=1.0): cv.float_,
        cv.Optional(CONF_OFFSET, default=0.0): cv.float_,
    }
)

CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    # The engines are ESP-IDF components from the registry and the component is
    # built and tested with esp-idf only; arduino-esp32 is not exercised.
    cv.only_with_framework(Framework.ESP_IDF),
    # The pinned esp_tsdb release declares targets ESP32, S2, S3, C3, C6, H2 and P4;
    # any other variant would pass validation and then fail dependency resolution.
    only_on_variant(
        supported=[
            VARIANT_ESP32,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32C3,
            VARIANT_ESP32C6,
            VARIANT_ESP32H2,
            VARIANT_ESP32P4,
        ],
        msg_prefix="tsdb",
    ),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TsdbComponent),
            cv.Optional(CONF_FILE, default="history.tsdb"): cv.All(
                cv.string_strict,
                cv.Length(min=1, max=FILE_NAME_MAX_BYTES),
                _utf8_bytes(FILE_NAME_MAX_BYTES),
                _validate_file_name,
            ),
            cv.Optional(CONF_MOUNT_POINT, default="/littlefs"): cv.All(
                cv.string_strict,
                cv.Length(min=2, max=MOUNT_POINT_MAX_BYTES),
                _utf8_bytes(MOUNT_POINT_MAX_BYTES),
                _validate_mount_point,
            ),
            cv.Optional(CONF_PARTITION, default="littlefs"): cv.All(
                cv.string_strict, cv.Length(min=1, max=15)
            ),
            # cv.string ahead of each size validator types it for the language schema.
            cv.Optional(CONF_PARTITION_SIZE, default="512KB"): cv.All(
                cv.string, _validate_partition_size
            ),
            cv.Optional(CONF_FORMAT_ON_FIRST_BOOT, default=True): cv.boolean,
            cv.Optional(CONF_RECREATE_ON_SCHEMA_CHANGE, default=False): cv.boolean,
            cv.Optional(CONF_MAX_FILE_SIZE, default="224KB"): cv.All(
                cv.string, _validate_size
            ),
            cv.Optional(
                CONF_INDEX_STRIDE, default=TSDB_DEFAULT_INDEX_STRIDE
            ): cv.int_range(min=1, max=100000),
            cv.Optional(CONF_BUFFER_POOL_SIZE, default="4KB"): cv.All(
                cv.string, _validate_buffer_pool_size, cv.Range(max=256 * 1024)
            ),
            cv.Optional(CONF_MEMORY, default="auto"): _validate_memory,
            cv.Optional(CONF_PAGED_ALLOCATION, default=False): cv.boolean,
            cv.Optional(CONF_PAGE_SIZE, default=TSDB_DEFAULT_PAGE_SIZE): cv.All(
                cv.string,
                _validate_size,
                cv.Range(min=TSDB_BLOCK_BYTES, max=64 * 1024),
            ),
            cv.Optional(
                CONF_SYNC_INTERVAL, default="15min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MIN_FREE_BYTES, default="32KB"): cv.All(
                cv.string, _validate_min_free_bytes
            ),
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
            # No default: setting it also compiles the dump in for lambda users.
            cv.Optional(CONF_DUMP_ROWS): cv.int_range(min=1, max=1000),
            cv.Required(CONF_COLUMNS): cv.All(
                cv.ensure_list(COLUMN_SCHEMA),
                cv.Length(min=1, max=TSDB_MAX_BASE_PARAMS),
            ),
        }
    ).extend(cv.polling_component_schema("60s")),
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


def _final_validate(config: ConfigType) -> None:
    if (
        not config[CONF_REQUIRE_TIME]
        or CONF_TIME_ID in config
        or CONF_TIME in fv.full_config.get()
    ):
        return
    raise cv.Invalid(
        "require_time needs a time source: add a time component or set require_time: false"
    )


FINAL_VALIDATE_SCHEMA = _final_validate


def _contains_key(node: object, key: str) -> bool:
    """Whether the validated configuration holds `key` anywhere, as an action name does."""
    if isinstance(node, dict):
        return key in node or any(_contains_key(value, key) for value in node.values())
    if isinstance(node, list):
        return any(_contains_key(item, key) for item in node)
    return False


def _uses_aggregates() -> bool:
    """Whether a tsdb sensor platform asks for an average, minimum or maximum."""
    return any(
        key in column
        for entry in CORE.config.get(CONF_SENSOR, [])
        if entry.get(CONF_PLATFORM) == DOMAIN
        for column in entry.get(CONF_COLUMNS, [])
        for key in ("average", "min", "max")
    )


async def to_code(config: ConfigType) -> None:
    for name, ref in IDF_COMPONENTS.items():
        add_idf_component(name=name, ref=ref)
    add_partition(
        config[CONF_PARTITION], "data", "littlefs", config[CONF_PARTITION_SIZE]
    )
    require_vfs_dir()
    add_idf_sdkconfig_option("CONFIG_LITTLEFS_WDT_RESET", True)

    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_FILE],
        config[CONF_MOUNT_POINT],
        config[CONF_PARTITION],
    )
    await cg.register_component(var, config)

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
    if _uses_aggregates():
        cg.add_define("USE_TSDB_AGGREGATES")
        cg.add(var.set_aggregate_window(config[CONF_AGGREGATE_WINDOW]))
        cg.add(var.set_aggregate_interval(config[CONF_AGGREGATE_INTERVAL]))
    if CONF_DUMP_ROWS in config or _contains_key(CORE.config, "tsdb.dump"):
        cg.add_define("USE_TSDB_DUMP")
        if (dump_rows := config.get(CONF_DUMP_ROWS)) is not None:
            cg.add(var.set_dump_rows(dump_rows))
    await cg.add_variable(var.set_time_source, config.get(CONF_TIME_ID))

    cg.add_define("TSDB_COLUMN_COUNT", len(config[CONF_COLUMNS]))
    for index, column in enumerate(config[CONF_COLUMNS]):
        # add_column() takes doubles: a plain float becomes a `1.0f` literal, which
        # underflows or overflows the scales the config accepts.
        scale = cg.RawExpression(repr(column[CONF_SCALE_FACTOR]))
        offset = cg.RawExpression(repr(column[CONF_OFFSET]))
        cg.add(var.add_column(column[CONF_NAME], scale, offset))
        cg.add(var.set_column_source(index, await cg.get_variable(column[CONF_SENSOR])))
