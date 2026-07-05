from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, CoroPriority, ID, coroutine_with_priority

CODEOWNERS = ["@p1ngb4ck"]

DOMAIN = "storage"

CONF_COPY_CHUNK_SIZE = "copy_chunk_size"
CONF_MAX_BLOCKING_TRANSFER_SIZE = "max_blocking_transfer_size"
CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_TASK_PRIORITY = "task_priority"
CONF_MAX_PENDING = "max_pending"

storage_ns = cg.esphome_ns.namespace("storage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)
StorageWorker = storage_ns.class_("StorageWorker", cg.Component)


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
#
# The task_*/max_pending keys only take effect when the async worker (storage_worker.h/.cpp,
# compiled in as USE_STORAGE_WORKER) is actually pulled in by a path-based driver, via that
# driver's own request_storage_worker() call in its to_code() (mirrors how sd_storage already
# calls request_storage_device()). If no such driver is configured, these keys are simply
# unused, same as any other config key with no effect in a given configuration.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageRegistry),
        cv.Optional(CONF_COPY_CHUNK_SIZE, default=16384): cv.All(
            cv.int_range(min=4096, max=131072), validate_sector_multiple
        ),
        # Guard-rail for the blocking copy/read/write helpers: 0 means unlimited (default,
        # preserves current behavior). See max_blocking_transfer_size's comment in storage.h.
        cv.Optional(CONF_MAX_BLOCKING_TRANSFER_SIZE, default=0): cv.int_range(min=0),
        # FATFS LFN + NFS/lwIP transfers both need headroom on the worker task's stack.
        cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(
            min=4096, max=32768
        ),
        # FreeRTOS priority: above idle (0), below networking tasks (typically higher).
        cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=1, max=23),
        # Fixed request pool/queue depth — sized exactly at codegen like the storage
        # registry's device count, no heap allocation per request at runtime.
        cv.Optional(CONF_MAX_PENDING, default=4): cv.int_range(min=1, max=16),
    }
)


@dataclass
class StorageData:
    device_count: int = 0
    worker_count: int = 0
    worker_task_safe: bool = False


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


def request_storage_worker(task_safe: bool = False) -> None:
    """Called by path-based drivers (Filesystem/NetworkStorage) that need the async worker.

    RawStorage drivers never call this, so on a raw-only node storage_worker.h/.cpp is not
    even compiled in (see USE_STORAGE_WORKER below) — zero RAM/flash cost for the feature.

    task_safe should be True only if the driver's data-plane calls are safe to run from a
    background FreeRTOS task for every instance it registers (e.g. SdMmc, which owns its bus
    exclusively) — not if that safety depends on how the bus is shared (e.g. SdSpi, which
    shares its bus with other devices). This aggregates via OR across all callers: if any
    driver requests task-safe operation, the worker creates its background task, which then
    also depends per-request on Storage::get_capabilities() reporting STORAGE_CAP_IO_TASK_SAFE.
    """
    data = _get_data()
    data.worker_count += 1
    if task_safe:
        data.worker_task_safe = True


# storage is a dependency of every driver and would otherwise run BEFORE them (default
# priority), reading device_count/worker_count as 0 — every driver's own to_code() is where
# request_storage_device()/request_storage_worker() actually get called. LATE (-100) runs
# after all default-priority driver to_code()s, so those counts are final by the time this
# reads them. Consumers awaiting the registry/worker variables (e.g. via cg.get_variable())
# are unaffected either way, since that call already suspends until the variable exists.
@coroutine_with_priority(CoroPriority.LATE)
async def to_code(config):
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    device_count = _get_data().device_count
    cg.add(var.set_device_count(device_count))

    cg.add(cg.RawExpression(f"{storage_ns}::global_storage_registry = {var}"))

    cg.add_define("USE_STORAGE_COPY_CHUNK_SIZE", config[CONF_COPY_CHUNK_SIZE])
    cg.add(var.set_max_blocking_transfer_size(config[CONF_MAX_BLOCKING_TRANSFER_SIZE]))

    data = _get_data()
    if data.worker_count > 0:
        cg.add_define("USE_STORAGE_WORKER")
        if data.worker_task_safe:
            cg.add_define("USE_STORAGE_WORKER_TASK")

        worker_id = ID(f"{var}_worker", is_declaration=True, type=StorageWorker)
        CORE.component_ids.add(str(worker_id))
        worker_var = cg.new_Pvariable(worker_id)
        await cg.register_component(worker_var, {})

        cg.add(worker_var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))
        cg.add(worker_var.set_task_priority(config[CONF_TASK_PRIORITY]))
        cg.add(worker_var.set_max_pending(config[CONF_MAX_PENDING]))

        cg.add(cg.RawExpression(f"{storage_ns}::global_storage_worker = {worker_var}"))
