from collections.abc import Callable
import re
from typing import Any

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType

CODEOWNERS = ["@clydebarrow"]
DOMAIN = "storage"
IS_PLATFORM_COMPONENT = True

CONF_BLOCK_SIZE = "block_size"
CONF_STORAGE_ID = "storage_id"

storage_ns = cg.esphome_ns.namespace("storage")
BlockDevice = storage_ns.class_("BlockDevice")

STORAGE_ID_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_STORAGE_ID): cv.use_id(BlockDevice),
    }
)

automation.register_apply_condition(
    "storage.has_media",
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(BlockDevice)}),
    "has_media()",
)

_SIZE_UNITS = {"": 1, "K": 1024, "M": 1024 * 1024, "G": 1024 * 1024 * 1024}


def _parse_size(value: str) -> int:
    match = re.match(r"^\s*(\d+)\s*([KMG]?)(?:i?B)?\s*$", value, re.IGNORECASE)
    if match is None:
        raise cv.Invalid(f"Expected a size such as 4096, 64KB or 1MB, got '{value}'")
    return cv.positive_not_null_int(
        int(match.group(1)) * _SIZE_UNITS[match.group(2).upper()]
    )


def _check_block_size(value: int) -> int:
    if value < 128 or value > 65536 or value & (value - 1):
        raise cv.Invalid("Block size must be a power of two from 128 to 65536")
    return value


# A size in bytes, given as an integer or with a binary suffix such as 64KB or 1MB. Storage sizes count in
# powers of two, so KB here is 1024 bytes, unlike cv.validate_bytes. cv.string comes first so that the
# language schema shows a string.
storage_size = cv.All(cv.string, _parse_size)
# A block size: a power of two from 128 to 64KB.
block_size = cv.All(storage_size, _check_block_size)


def storage_size_range(
    min_size: int = 1, max_size: int | None = None
) -> Callable[[Any], int]:
    """storage_size limited to a range. Unlike cv.int_range, it keeps the string type in the language schema."""

    def check(value: int) -> int:
        if value < min_size:
            raise cv.Invalid(f"The size must be at least {min_size} bytes")
        if max_size is not None and value > max_size:
            raise cv.Invalid(f"The size must be at most {max_size} bytes")
        return value

    return cv.All(storage_size, check)


def request_lock() -> None:
    """Make block device calls safe from several tasks, such as a filesystem used through the C library.

    Call it from to_code.
    """
    cg.add_define("USE_STORAGE_LOCK")


def block_device_schema(class_: MockObjClass) -> cv.Schema:
    """The base schema for a block device platform; `extend` it with the platform's own options."""
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
        }
    ).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_STORAGE")
