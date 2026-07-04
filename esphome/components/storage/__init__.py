from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE

CODEOWNERS = ["@p1ngb4ck"]

DOMAIN = "storage"

CONF_COPY_CHUNK_SIZE = "copy_chunk_size"

storage_ns = cg.esphome_ns.namespace("storage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)


def validate_sector_multiple(value):
    """Require a multiple of 512 (the common sector size).

    Anything else loses the FATFS direct-sector-read path that motivated picking a
    16kB chunk size in the first place — see STORAGE_COPY_CHUNK_SIZE's comment in storage.h.
    """
    if value % 512 != 0:
        raise cv.Invalid(f"copy_chunk_size must be a multiple of 512, got {value}")
    return value


# Default kept in sync with the STORAGE_COPY_CHUNK_SIZE fallback in storage.h.
# Lower bound matches copy()'s allocation fallback floor (4096, see storage.cpp); upper bound
# is a sanity cap so a typo (e.g. "16MB") can't request an unreasonable single allocation.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageRegistry),
        cv.Optional(CONF_COPY_CHUNK_SIZE, default="16kB"): cv.All(
            cv.validate_bytes,
            cv.int_range(min=4096, max=131072),
            validate_sector_multiple,
        ),
    }
)


@dataclass
class StorageData:
    device_count: int = 0


def _get_data() -> StorageData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = StorageData()
    return CORE.data[DOMAIN]


def request_storage_device() -> None:
    """Called by each storage driver's to_code() to count configured devices.

    The accumulated count is passed to StorageRegistry.set_device_count() so the
    internal FixedVector is sized exactly — no compile-time upper bound needed.
    """
    _get_data().device_count += 1


async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    device_count = _get_data().device_count
    cg.add(var.set_device_count(device_count))

    cg.add(cg.RawExpression(f"{storage_ns}::global_storage_registry = {var}"))

    cg.add_define("STORAGE_COPY_CHUNK_SIZE", config[CONF_COPY_CHUNK_SIZE])
