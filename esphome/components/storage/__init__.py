import re

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CoroPriority, coroutine_with_priority
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, SafeExpType

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


def storage_size(value: object) -> int:
    """Validate a size in bytes, given as an integer or with a binary suffix such as 64KB or 1MB.

    Storage sizes count in powers of two, so KB here is 1024 bytes, unlike cv.validate_bytes.
    """
    if isinstance(value, int):
        return cv.positive_not_null_int(value)
    value = cv.string(value)
    if (
        match := re.match(r"^\s*(\d+)\s*([KMG]?)(?:i?B)?\s*$", value, re.IGNORECASE)
    ) is None:
        raise cv.Invalid(f"Expected a size such as 4096, 64KB or 1MB, got '{value}'")
    return cv.positive_not_null_int(
        int(match.group(1)) * _SIZE_UNITS[match.group(2).upper()]
    )


def block_size(value: object) -> int:
    """Validate a block size: a power of two from 128 to 64KB."""
    value = storage_size(value)
    if value < 128 or value > 65536 or value & (value - 1):
        raise cv.Invalid("Block size must be a power of two from 128 to 65536")
    return value


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


async def new_block_device(config: ConfigType, *args: SafeExpType) -> MockObj:
    """Create a block device platform component and register it."""
    var = cg.new_Pvariable(config[CONF_ID], *args)
    await cg.register_component(var, config)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_STORAGE")
