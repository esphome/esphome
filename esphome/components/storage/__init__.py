"""
Storage component for ESPHome.

WARNING: This component is EXPERIMENTAL. The API (both Python configuration
and C++ interfaces) may change at any time without following the normal
breaking changes policy. Use at your own risk.

Once the API is considered stable, this warning will be removed.
"""

from dataclasses import dataclass
import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, ID, CoroPriority, EsphomeError, coroutine_with_priority
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@p1ngb4ck"]

_LOGGER = logging.getLogger(__name__)

DOMAIN = "storage"

CONF_COPY_CHUNK_SIZE = "copy_chunk_size"
CONF_PATH_MAX = "path_max"
CONF_MAX_BLOCKING_TRANSFER_SIZE = "max_blocking_transfer_size"
CONF_MOVE_FALLBACK_COPY = "move_fallback_copy"
# The key every storage driver uses for its mount point, so that _final_validate()
# below finds them all without each driver having to cooperate.
CONF_MOUNT_PATH = "mount_path"

CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_TASK_PRIORITY = "task_priority"
CONF_MAX_PENDING = "max_pending"
CONF_MAX_STREAMS = "max_streams"
CONF_WORKER_UPDATE_INTERVAL = "worker_update_interval"
CONF_WORKER_ID = "worker_id"

# Not yet in esphome/const.py
CONF_ON_REGISTERED = "on_registered"
CONF_ON_UNREGISTERED = "on_unregistered"


storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.Component)
StoragePtr = Storage.operator("ptr")
PathStorage = storage_ns.class_("PathStorage", Storage)
RawStorage = storage_ns.class_("RawStorage", Storage)
MountableStorage = storage_ns.class_("MountableStorage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)
StorageWorker = storage_ns.class_("StorageWorker", cg.PollingComponent)


def validate_sector_multiple(value: int) -> int:
    """Require a multiple of 512 (the common sector size).

    Anything else loses the FATFS direct-sector-read path that motivated picking a
    16kB chunk size in the first place -- see STORAGE_COPY_CHUNK_SIZE's comment in storage.h.
    """
    if value % 512 != 0:
        raise cv.Invalid(f"copy_chunk_size must be a multiple of 512, got {value}")
    return value


# Default kept in sync with the STORAGE_COPY_CHUNK_SIZE fallback in storage.h. Lower bound matches
# copy()'s allocation floor (4096, storage.cpp); upper bound is a sanity cap against a typo
# requesting an unreasonable single allocation.
#
# The task_*/max_pending keys take effect only when the async worker (USE_STORAGE_WORKER) is pulled
# in by a path-based driver via its own request_storage_worker() in to_code() (mirrors sd_storage's
# request_storage_device()). With no such driver they are simply unused.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageRegistry),
        # The async worker is a second component minted from this same storage: block. Declaring its
        # id here lets validation register it like any component, so the component count includes it;
        # it is only built (new_Pvariable) when a driver pulls the worker in via
        # request_storage_worker().
        cv.GenerateID(CONF_WORKER_ID): cv.declare_id(StorageWorker),
        # No static default: an absent value means "use the per-platform default"
        # (see _default_copy_chunk_size() / to_code). An explicit value overrides it and
        # is still range- and sector-checked here.
        cv.Optional(CONF_COPY_CHUNK_SIZE): cv.All(
            cv.int_range(min=4096, max=131072), validate_sector_multiple
        ),
        # Longest relative path the API carries. No static default: absent means "the largest any
        # configured driver asked for" (see request_path_length / to_code). Raising it also raises
        # the tree walks' stack use (two buffers per level), so the range is bounded and
        # _validate_walk_budget() below checks it against task_stack_size.
        cv.Optional(CONF_PATH_MAX): cv.int_range(min=64, max=1024),
        # Guard-rail for the blocking copy/read/write helpers, which hold the whole payload in RAM.
        # storage.file_read takes whatever size the file happens to be -- on a node without PSRAM,
        # the one storage action whose cost the author does not choose; anything bigger belongs on
        # the worker. It also bounds storage.preferences_export/import (same helpers); past the
        # ceiling, narrow with the action's `preferences:` filter. 0 disables. See storage.h.
        cv.Optional(CONF_MAX_BLOCKING_TRANSFER_SIZE, default=16384): cv.All(
            cv.validate_bytes, cv.int_range(min=0)
        ),
        # A same-storage move is a rename, which some backends refuse across their own internals
        # (an NFS export can span file systems, and RENAME never crosses one). On: redo the refusal
        # as copy + remove so the move still happens; off: report it instead of quietly turning a
        # directory-entry update into a full copy.
        cv.Optional(CONF_MOVE_FALLBACK_COPY, default=True): cv.boolean,
        # FATFS LFN + NFS/lwIP transfers both need headroom on the worker task's stack.
        cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(
            min=4096, max=32768
        ),
        # FreeRTOS priority: above idle (0), below networking tasks (typically higher).
        cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=1, max=23),
        # Fixed request pool/queue depth -- sized exactly at codegen like the storage
        # registry's device count, so the slot itself never allocates at runtime. (The
        # completion callback is a std::function and may allocate for a large lambda capture.)
        cv.Optional(CONF_MAX_PENDING, default=4): cv.int_range(min=1, max=16),
        # Fixed stream pool depth (begin_write()/begin_read() and friends, storage_worker.h) --
        # streams are typically much longer-lived than a single copy/move (e.g. one HTTP
        # upload in progress), so a node doing one at a time needs very few slots.
        cv.Optional(CONF_MAX_STREAMS, default=2): cv.int_range(min=1, max=8),
        # How often the async worker's engine runs (PollingComponent update_interval). The
        # worker is driven by this scheduler interval, not the gated component loop(); a small
        # value keeps chunked transfers moving at a good rate without busy-spinning the CPU.
        cv.Optional(
            CONF_WORKER_UPDATE_INTERVAL, default="5ms"
        ): cv.positive_time_period_milliseconds,
        # Fired for every storage device, not just file-browser consumers -- any component that
        # cares about hotplug/availability listens here instead of reinventing "storage changed".
        # See StorageRegistry::add_on_registered_callback()/add_on_unregistered_callback() in
        # storage.h.
        cv.Optional(CONF_ON_REGISTERED): automation.validate_automation({}),
        cv.Optional(CONF_ON_UNREGISTERED): automation.validate_automation({}),
    }
)


def _collect_mount_paths(
    fragment: object, where: str, out: list[tuple[str, str]]
) -> None:
    """Walk a validated config fragment, collecting the mount point of every storage device.

    A storage device is identified by its own id, not by a key name: cv.declare_id gives it a type
    that inherits from Storage. mount_path is collected only from such a node. A foreign component
    that happens to use a key literally named 'mount_path' is not a storage device, and there is no
    way to enumerate every external component to exclude, so we key off the device's own identity
    rather than reserving the name 'mount_path' across the whole config.
    """
    if isinstance(fragment, dict):
        node_id = fragment.get(CONF_ID)
        mount = fragment.get(CONF_MOUNT_PATH)
        if (
            isinstance(node_id, ID)
            and node_id.type is not None
            and node_id.type.inherits_from(Storage)
            and isinstance(mount, str)
        ):
            out.append((mount, where))
        for value in fragment.values():
            _collect_mount_paths(value, where, out)
    elif isinstance(fragment, list):
        for item in fragment:
            _collect_mount_paths(item, where, out)


def _final_validate(config: ConfigType) -> ConfigType:
    """Reject two storage devices claiming the same mount point.

    This lives here and not in the drivers because no driver can see the others' configuration.
    A mount point is a name in one shared namespace, the one resolve_path() searches, and only
    the component owning that namespace can tell whether a name was taken twice. A driver
    checking just its own instances catches the easy half and misses an SD card and an NFS
    share both sitting on /sdcard.

    Reads the validated config and writes nothing. The codegen-side bookkeeping is
    register_mount_path(), which drivers call from their to_code().
    """
    seen: dict[str, str] = {}
    for domain, fragment in fv.full_config.get().items():
        found: list[tuple[str, str]] = []
        _collect_mount_paths(fragment, domain, found)
        for path, where in found:
            other = seen.get(path)
            if other is None:
                seen[path] = where
                continue
            if other == where:
                raise cv.Invalid(
                    f"Mount path '{path}' is claimed twice within '{where}'. "
                    f"Each storage device needs its own mount point."
                )
            raise cv.Invalid(
                f"Mount path '{path}' is claimed by both '{other}' and '{where}'. "
                f"Each storage device needs its own mount point."
            )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


@dataclass
class StorageData:
    device_count: int = 0
    # Longest mount point any driver registered, see register_mount_path().
    mount_path_max: int = 0
    # Largest path length any configured driver reported via request_path_length().
    path_max: int = 0
    # Set by FATFS-backed drivers, whose bound is not a constant but whatever
    # CONFIG_FATFS_MAX_LFN ends up being -- see request_fatfs_path_length().
    fatfs_path_bound: bool = False
    worker_count: int = 0
    worker_task_safe: bool = False


def _get_data() -> StorageData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = StorageData()
    return CORE.data[DOMAIN]


def request_storage_device() -> None:
    """Called by each storage driver's to_code() to count configured devices.

    The accumulated count is passed to StorageRegistry.set_device_count() so the
    internal FixedVector is sized exactly -- no compile-time upper bound needed.
    """
    _get_data().device_count += 1


def request_path_length(length: int) -> None:
    """Called by each storage driver's to_code() with the longest relative path it can carry.

    The API sizes its own buffers to the largest of these, so a driver with a tighter limit of
    its own still refuses over-long paths itself -- that is the driver's business, not this
    bound's. An explicit `path_max:` in YAML overrides the collected value.
    """
    data = _get_data()
    data.path_max = max(data.path_max, length)


def request_fatfs_path_length() -> None:
    """Called by drivers whose paths are bounded by FATFS long filenames.

    Their limit is not a constant: CONFIG_FATFS_MAX_LFN is a sdkconfig option the esp32
    component sets a default for and the user can override. Resolving it here at codegen time
    (rather than baking 255 into the driver) means a user who lowers it to save flash gets an
    API bound that matches, and one who has no FATFS driver at all is unaffected.
    """
    _get_data().fatfs_path_bound = True


def validate_mount_path(value: str) -> str:
    """Validate a storage device's mount point. Drivers use this in place of cv.string.

    The interface treats the mount path as an invariant: set once at construction time and
    never changed, so resolve_path() and build_path() in storage.cpp assume its shape instead
    of re-checking it per call (see get_mount_path() in storage.h). That assumption is only
    worth anything if it is enforced here.

    Mount points live at the root level: exactly one segment, e.g. "/sd". A nested mount point
    ("/sd/nested") would resolve correctly, since resolve_path() prefers the longest match, but
    nothing else in the stack knows that mount points exist. list_dir() and the tree walks in
    copy()/remove_recursive() operate inside a single storage, so a directory of the same name
    on the outer device would be shadowed for path lookups while the walks kept seeing it,
    which is two different answers to the same path.
    """
    value = cv.string_strict(value)
    if not value.startswith("/"):
        raise cv.Invalid(f"Mount path must start with '/', got '{value}'")
    if value == "/":
        raise cv.Invalid(
            "Mount path must name a directory of its own; '/' is not a mount point"
        )
    if value.endswith("/"):
        raise cv.Invalid(f"Mount path must not end with '/', got '{value}'")
    if "/" in value[1:]:
        raise cv.Invalid(
            f"Mount path must be a single root-level segment such as '/sd', got '{value}'. "
            f"Nested mount points are not supported."
        )
    return value


def register_mount_path(path: str) -> None:
    """Called by each storage driver's to_code() with the mount point it configured.

    A full VFS path is the mount point plus a relative path (StorageRegistry::build_path() in
    storage.cpp), so it is longer than the relative paths request_path_length() bounds, and
    codegen is the only place that knows by how much, because mount points are configuration.
    The collected maximum sizes USE_STORAGE_VFS_PATH_MAX so a buffer holding a full path is not
    one mount point too small. Getting that wrong is quiet rather than loud: build_path()
    refuses to write instead of overflowing, and its callers mostly test the return value
    without an else branch.

    Uniqueness is not this function's business. Two devices on one mount point is a
    configuration error, rejected during validation; see FINAL_VALIDATE_SCHEMA above.
    """
    data = _get_data()
    data.mount_path_max = max(data.mount_path_max, len(path))


def request_storage_worker(task_safe: bool = False) -> None:
    """Called by drivers that need the async worker; path-based ones always do.

    A RawStorage driver normally does not, which is what keeps storage_worker.h/.cpp out of a
    raw-only build entirely (see USE_STORAGE_WORKER below) -- zero RAM and flash for a node
    that has nothing to run on it.

    The exception is a raw device the user has declared alone on its bus: binary_storage's
    assume_exclusive_bus lets an SPI or I2C chip be driven from the background task, and its
    final validation refuses the promise unless the device really is the only thing on that
    bus and the bus is a hardware bus on esp32. Such a device calls this too, because that
    background task has to exist for it to be used.

    task_safe should be True only where the driver's data-plane calls are safe to run from a
    background FreeRTOS task for every instance it registers. That is a property of the
    hardware arrangement, not of the driver: SdMmc owns its controller and passes True
    unconditionally, SdSpi shares a general SPI bus and passes False, and a binary_storage chip
    passes True only once the exclusive-bus promise has been checked.

    This aggregates via OR across all callers: if any driver requests task-safe operation, the
    worker creates its background task, which then still decides per request from
    Storage::get_capabilities() reporting STORAGE_CAP_IO_TASK_SAFE.
    """
    data = _get_data()
    data.worker_count += 1
    if task_safe:
        data.worker_task_safe = True


# Default streaming/copy chunk size. Flat 16 kB everywhere: the 20 ms loop-slice budget caps a
# main-loop chunk near 16 kB even on the fastest S3 SD path, so a larger loop chunk is unsafe. The
# platform distinction lives one level down in the C++ allocator (alloc_dma_capable): the worker
# task (no 20 ms budget) stages a larger DMA-capable PSRAM chunk on S3/P4, loop-path buffers stay
# 16 kB internal. An explicit copy_chunk_size overrides this. Multiple of 512 for FATFS
# whole-sector transfers.
_DEFAULT_COPY_CHUNK_SIZE = 16384


# Fallback when no driver reported anything (storage configured without a device).
# Mirrors STORAGE_PATH_MAX's compile-time fallback in storage.h.
_DEFAULT_PATH_MAX = 256

# The copy walk's two path buffers are allocated once and shared by every level (append_path_segment
# in storage.cpp), a flat cost. What scales with depth is the walk's own frame plus the driver's
# list_dir()/remove() frames (FATFS LFN buffers dominate). Keep 25% of the task stack free for the
# caller.
_WALK_DRIVER_STACK_PER_LEVEL = 830
_WALK_STACK_HEADROOM = 0.75

# Max directory nesting the tree walks descend into. Emitted as a define so storage.h and the
# budget check below cannot drift apart.
_MAX_RECURSION_DEPTH = 4


def _walk_stack_bytes(path_max: int, depth: int) -> int:
    """Worst-case stack the tree walks need for `depth` levels of recursion."""
    return 2 * path_max + (depth + 1) * _WALK_DRIVER_STACK_PER_LEVEL


# ESP-IDF's own default when nothing sets CONFIG_FATFS_MAX_LFN; the esp32 component applies
# the same value from _reconcile_vfs_fatfs_sdkconfig(). Only read as a fallback for the case
# where that has not run at all (no FATFS driver -> the option is never touched).
_FATFS_MAX_LFN_DEFAULT = 255

# 8.3 plus the dot and the terminator -- the longest name FatFs produces without long-filename
# support, which a user can switch off to save flash.
_FATFS_SHORT_NAME_MAX = 13


def _resolve_path_max(config: ConfigType) -> int:
    """The API's path bound, resolved once every contributor has had its say.

    An explicit `path_max:` wins. Otherwise it is the largest bound any configured driver
    needs -- the maximum, not the minimum: the buffers have to carry the longest path any of
    them accepts, and a driver with a tighter limit of its own refuses over-long paths itself.
    FATFS-backed drivers contribute CONFIG_FATFS_MAX_LFN rather than a constant, which is why
    this runs at FINAL - 1: esp32 reconciles that option at FINAL.
    """
    if (explicit := config.get(CONF_PATH_MAX)) is not None:
        return explicit
    data = _get_data()
    # Only what drivers actually reported counts. _DEFAULT_PATH_MAX is the answer when nobody
    # did (storage configured without a device), not a floor under the derivation -- used as
    # one it would swallow a lowered CONFIG_FATFS_MAX_LFN and make this whole resolution moot.
    bounds = []
    if data.path_max > 0:
        bounds.append(data.path_max)
    if data.fatfs_path_bound and CORE.is_esp32:
        from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS

        opts = CORE.data.get(KEY_ESP32, {}).get(KEY_SDKCONFIG_OPTIONS, {})
        # Long filenames off: FatFs hands back 8.3 names, and CONFIG_FATFS_MAX_LFN is not
        # written at all in that case -- reading it would fall back to 255 and size every
        # buffer twenty times larger than anything the medium can produce.
        lfn_off = opts.get("CONFIG_FATFS_LFN_NONE")
        lfn_off = getattr(lfn_off, "value", lfn_off)
        if str(lfn_off).strip().lower() in ("y", "true", "1"):
            # 8.3 names only: STORAGE_PATH_MAX bounds the whole relative path (append_path_segment
            # builds "/<name>" per level into one shared buffer), not a single filename. The walks
            # descend to STORAGE_MAX_RECURSION_DEPTH and the callback appends a level's segment
            # before the depth guard rejects the next one, so the deepest path a walk builds relative
            # to its starting dir is _MAX_RECURSION_DEPTH + 1 segments of "/<short name>" plus the
            # terminator. The starting dir is not added on top: it is already bounded by
            # STORAGE_PATH_MAX, and append_path_segment returns 0 -- a clean INVALID_ARGS, not an
            # overrun -- if base + walk ever exceeds the buffer, so this sizes for the common walk
            # from a shallow base rather than reserving room for an arbitrarily deep one.
            bounds.append(_FATFS_SHORT_NAME_MAX * (_MAX_RECURSION_DEPTH + 1) + 1)
            return max(bounds)
        lfn = opts.get("CONFIG_FATFS_MAX_LFN", _FATFS_MAX_LFN_DEFAULT)
        # A YAML sdkconfig_options entry arrives wrapped so it is written out verbatim; the
        # esp32 component's own default is a plain int. Both carry the same number.
        lfn = getattr(lfn, "value", lfn)
        try:
            # A name plus its terminator is the longest single component FATFS will hand back.
            bounds.append(int(lfn) + 1)
        except (TypeError, ValueError) as exc:
            # A non-numeric CONFIG_FATFS_MAX_LFN is a configuration error, not a value to guess a
            # bound for: a wrong guess sizes USE_STORAGE_PATH_MAX too small and fails legitimate
            # paths with INVALID_ARGS only at runtime on the device.
            raise EsphomeError(
                f"storage: CONFIG_FATFS_MAX_LFN is {lfn!r}, which is not a number -- set it to the "
                "FatFs long-filename limit (an integer) in sdkconfig_options"
            ) from exc
    return max(bounds) if bounds else _DEFAULT_PATH_MAX


def _default_copy_chunk_size() -> int:
    """The loop-safe base chunk size (platform-independent -- see the note above)."""
    return _DEFAULT_COPY_CHUNK_SIZE


# storage is a dependency of every driver and would otherwise run BEFORE them (default priority),
# reading device_count/worker_count as 0 -- each driver's own to_code() is where
# request_storage_device()/request_storage_worker() are called. LATE (-100) runs after all
# default-priority driver to_code()s, so those counts are final here. Consumers awaiting the
# registry/worker variables (cg.get_variable()) are unaffected -- that call already suspends until
# the variable exists.
@coroutine_with_priority(CoroPriority.LATE)
async def to_code(config: ConfigType) -> None:
    _LOGGER.warning(
        "The storage component is experimental; its configuration and C++ API may change "
        "without following the normal breaking-changes policy."
    )
    var = cg.new_Pvariable(config[cv.GenerateID()])
    await cg.register_component(var, config)

    device_count = _get_data().device_count
    cg.add(var.set_device_count(device_count))
    # Compile-time bound for the enumeration snapshot in StorageRegistry::for_each*. Only emit it
    # when a driver registered a device; with none (every tests/components/storage/ config) storage.h
    # keeps its own >0 fallback, so the header's "fallback unreachable in real builds" stays true.
    if device_count > 0:
        cg.add_define("USE_STORAGE_MAX_DEVICES", device_count)

    cg.add(cg.RawExpression(f"{storage_ns}::global_storage_registry = {var}"))

    cg.add_define("USE_STORAGE")
    cg.add_define("USE_STORAGE_MAX_RECURSION_DEPTH", _MAX_RECURSION_DEPTH)

    # The path bound cannot be settled here: a FATFS driver contributes CONFIG_FATFS_MAX_LFN,
    # which the esp32 component reconciles at FINAL. Emit it from behind that.
    @coroutine_with_priority(CoroPriority.FINAL - 1)
    async def _emit_path_max():
        path_max = _resolve_path_max(config)
        cg.add_define("USE_STORAGE_PATH_MAX", path_max)
        # A full VFS path carries the mount point too: mount + '/' + relative + NUL, with
        # path_max already covering the relative part's terminator.
        cg.add_define(
            "USE_STORAGE_VFS_PATH_MAX", path_max + _get_data().mount_path_max + 1
        )

        # task_stack_size sizes the worker task, which exists only when a task-safe driver pulled
        # it in (USE_STORAGE_WORKER_TASK). With only non-task-safe drivers the walk runs on the
        # main-loop stack and task_stack_size is inert, so budget-check it only when that task is
        # really created -- otherwise a lowered task_stack_size hard-fails the build citing a stack
        # that never exists.
        if _get_data().worker_task_safe:
            needed = _walk_stack_bytes(path_max, _MAX_RECURSION_DEPTH)
            raw = config[CONF_TASK_STACK_SIZE]
            budget = int(raw * _WALK_STACK_HEADROOM)
            if needed > raw:
                # Runs in a codegen job (path_max can depend on a FATFS bound the esp32 component
                # reconciles at FINAL), not validation, so cv.Invalid would escape and reach the user
                # as a raw traceback. EsphomeError is caught by the CLI and logged cleanly.
                raise EsphomeError(
                    f"storage: a {_MAX_RECURSION_DEPTH}-level tree walk with path_max {path_max} "
                    f"needs roughly {needed} bytes of stack, which exceeds task_stack_size ({raw}) "
                    f"and would overflow it at runtime. Raise task_stack_size."
                )
            if needed > budget:
                _LOGGER.warning(
                    "storage: a %d-level tree walk with path_max %d needs roughly %d bytes of "
                    "stack, leaving little headroom in task_stack_size (%d). Raise "
                    "task_stack_size if deep copy/remove operations misbehave.",
                    _MAX_RECURSION_DEPTH,
                    path_max,
                    needed,
                    raw,
                )

    CORE.add_job(_emit_path_max)
    # Absent copy_chunk_size -> per-platform default; explicit value overrides.
    copy_chunk_size = config.get(CONF_COPY_CHUNK_SIZE) or _default_copy_chunk_size()
    cg.add_define("USE_STORAGE_COPY_CHUNK_SIZE", copy_chunk_size)
    cg.add(var.set_max_blocking_transfer_size(config[CONF_MAX_BLOCKING_TRANSFER_SIZE]))
    cg.add(var.set_move_fallback_copy(config[CONF_MOVE_FALLBACK_COPY]))

    for conf in config.get(CONF_ON_REGISTERED, []):
        await automation.build_callback_automation(
            var, "add_on_registered_callback", [(StoragePtr, "x")], conf
        )
    for conf in config.get(CONF_ON_UNREGISTERED, []):
        await automation.build_callback_automation(
            var, "add_on_unregistered_callback", [(StoragePtr, "x")], conf
        )

    data = _get_data()
    if data.worker_count > 0:
        cg.add_define("USE_STORAGE_WORKER")
        if data.worker_task_safe:
            cg.add_define("USE_STORAGE_WORKER_TASK")

        worker_var = cg.new_Pvariable(config[CONF_WORKER_ID])
        await cg.register_component(worker_var, {})

        cg.add(worker_var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))
        cg.add(worker_var.set_task_priority(config[CONF_TASK_PRIORITY]))
        cg.add(worker_var.set_max_pending(config[CONF_MAX_PENDING]))
        cg.add(worker_var.set_max_streams(config[CONF_MAX_STREAMS]))
        cg.add(worker_var.set_update_interval(config[CONF_WORKER_UPDATE_INTERVAL]))

        cg.add(cg.RawExpression(f"{storage_ns}::global_storage_worker = {worker_var}"))
