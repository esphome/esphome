from dataclasses import dataclass

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, coroutine_with_priority

CODEOWNERS = ["@p1ngb4ck"]

DOMAIN = "storage"

CONF_COPY_CHUNK_SIZE = "copy_chunk_size"
CONF_MAX_BLOCKING_TRANSFER_SIZE = "max_blocking_transfer_size"

# Not yet in esphome/const.py
CONF_ON_REGISTERED = "on_registered"
CONF_ON_UNREGISTERED = "on_unregistered"

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.Component)
StoragePtr = Storage.operator("ptr")
PathStorage = storage_ns.class_("PathStorage", Storage)
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
# is a sanity cap so a typo can't request an unreasonable single allocation (e.g. 16777216).
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageRegistry),
        cv.Optional(CONF_COPY_CHUNK_SIZE, default=16384): cv.All(
            cv.int_range(min=4096, max=131072), validate_sector_multiple
        ),
        # Guard-rail for the blocking copy/read/write helpers: 0 means unlimited (default,
        # preserves current behavior). See max_blocking_transfer_size's comment in storage.h.
        cv.Optional(CONF_MAX_BLOCKING_TRANSFER_SIZE, default=0): cv.int_range(min=0),
        # Fired for every storage device, not just file-browser-style consumers — any
        # component that cares about hotplug/availability can listen here instead of
        # each reinventing its own notion of "storage changed". See
        # StorageRegistry::add_on_registered_callback()/add_on_unregistered_callback()
        # in storage.h.
        cv.Optional(CONF_ON_REGISTERED): automation.validate_automation({}),
        cv.Optional(CONF_ON_UNREGISTERED): automation.validate_automation({}),
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


# storage is a dependency of every driver and would otherwise run BEFORE them (default
# priority), reading device_count as 0 — every driver's own to_code() is where
# request_storage_device() actually gets called. LATE (-100) runs after all
# default-priority driver to_code()s, so the count is final by the time this reads it.
# Consumers awaiting the registry variable (e.g. via cg.get_variable()) are unaffected
# either way, since that call already suspends until the variable exists.
@coroutine_with_priority(CoroPriority.LATE)
async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    device_count = _get_data().device_count
    cg.add(var.set_device_count(device_count))

    cg.add(cg.RawExpression(f"{storage_ns}::global_storage_registry = {var}"))

    cg.add_define("USE_STORAGE_COPY_CHUNK_SIZE", config[CONF_COPY_CHUNK_SIZE])
    cg.add(var.set_max_blocking_transfer_size(config[CONF_MAX_BLOCKING_TRANSFER_SIZE]))

    for conf in config.get(CONF_ON_REGISTERED, []):
        await automation.build_callback_automation(
            var, "add_on_registered_callback", [(StoragePtr, "x")], conf
        )
    for conf in config.get(CONF_ON_UNREGISTERED, []):
        await automation.build_callback_automation(
            var, "add_on_unregistered_callback", [(StoragePtr, "x")], conf
        )
