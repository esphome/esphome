from dataclasses import dataclass, field
import logging
import re

from esphome import automation, core
import esphome.codegen as cg
from esphome.components import globals as globals_
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ALL,
    CONF_ARGS,
    CONF_DATA,
    CONF_FORMAT,
    CONF_FROM,
    CONF_GROUP,
    CONF_ID,
    CONF_INDEX,
    CONF_KEY,
    CONF_ON_VALUE,
    CONF_PATH,
    CONF_SIZE,
    CONF_TO,
)
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv

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
CONF_ON_COMPLETE = "on_complete"

# Not yet in esphome/const.py
CONF_ON_REGISTERED = "on_registered"
CONF_ON_UNREGISTERED = "on_unregistered"

# json is header-only (ArduinoJson): auto-loading it costs nothing when unused
# and lets the json extract step and the preferences json format work without
# an explicit `json:` block in the config.
AUTO_LOAD = ["json"]

storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.Component)
StoragePtr = Storage.operator("ptr")
PathStorage = storage_ns.class_("PathStorage", Storage)
RawStorage = storage_ns.class_("RawStorage", Storage)
MountableStorage = storage_ns.class_("MountableStorage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)
StorageWorker = storage_ns.class_("StorageWorker", cg.PollingComponent)


def validate_sector_multiple(value):
    """Require a multiple of 512 (the common sector size).

    Anything else loses the FATFS direct-sector-read path that motivated picking a
    16kB chunk size in the first place -- see STORAGE_COPY_CHUNK_SIZE's comment in storage.h.
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
        # No static default: an absent value means "use the per-platform default"
        # (see _default_copy_chunk_size() / to_code). An explicit value overrides it and
        # is still range- and sector-checked here.
        cv.Optional(CONF_COPY_CHUNK_SIZE): cv.All(
            cv.int_range(min=4096, max=131072), validate_sector_multiple
        ),
        # Longest relative path the API carries. No static default: absent means "the largest
        # any configured driver asked for" (see request_path_length / to_code). Raising it also
        # raises the tree walks' stack use -- two buffers per recursion level -- so the range
        # is bounded and _validate_walk_budget() below checks it against task_stack_size.
        cv.Optional(CONF_PATH_MAX): cv.int_range(min=64, max=1024),
        # Guard-rail for the blocking copy/read/write helpers, which hold the whole payload
        # in RAM: storage.file_read takes whatever size the file happens to be, and on a node
        # without PSRAM that is the one storage action whose cost the automation author does
        # not choose. Anything bigger belongs on the worker (storage.file_copy, raw_write
        # from_file). It also bounds storage.preferences_export/import, which share these
        # helpers -- an export past the ceiling is a sign to narrow it with the action's
        # `preferences:` filter. 0 disables the check. See storage.h for the C++ side.
        cv.Optional(CONF_MAX_BLOCKING_TRANSFER_SIZE, default=16384): cv.int_range(
            min=0
        ),
        # A same-storage move is a rename, which some backends refuse across their own
        # internals (an NFS export can span file systems, and RENAME never crosses one).
        # On, such a refusal is redone as copy + remove so the move still happens; off, it is
        # reported instead of quietly turning a directory-entry update into a full copy.
        cv.Optional(CONF_MOVE_FALLBACK_COPY, default=True): cv.boolean,
        # FATFS LFN + NFS/lwIP transfers both need headroom on the worker task's stack.
        cv.Optional(CONF_TASK_STACK_SIZE, default=8192): cv.int_range(
            min=4096, max=32768
        ),
        # FreeRTOS priority: above idle (0), below networking tasks (typically higher).
        cv.Optional(CONF_TASK_PRIORITY, default=1): cv.int_range(min=1, max=23),
        # Fixed request pool/queue depth -- sized exactly at codegen like the storage
        # registry's device count, no heap allocation per request at runtime.
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
        # Fired for every storage device, not just file-browser-style consumers -- any
        # component that cares about hotplug/availability can listen here instead of
        # each reinventing its own notion of "storage changed". See
        # StorageRegistry::add_on_registered_callback()/add_on_unregistered_callback()
        # in storage.h.
        cv.Optional(CONF_ON_REGISTERED): automation.validate_automation({}),
        cv.Optional(CONF_ON_UNREGISTERED): automation.validate_automation({}),
    }
)


def _collect_mount_paths(fragment, where, out):
    """Walk a validated config fragment, collecting every (mount point, location) it declares."""
    if isinstance(fragment, dict):
        for key, value in fragment.items():
            if key == CONF_MOUNT_PATH and isinstance(value, str):
                out.append((value, where))
            else:
                _collect_mount_paths(value, where, out)
    elif isinstance(fragment, list):
        for item in fragment:
            _collect_mount_paths(item, where, out)


def _final_validate(config):
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
    # Raw preference regions per device id: every export/import action's address, plus the
    # container size when it can be computed. Filled while the actions are built and resolved
    # once at the end -- see _resolve_raw_pref_regions().
    raw_pref_regions: dict = field(default_factory=dict)
    raw_pref_job_queued: bool = False
    sensor_pref_job_queued: bool = False


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


def validate_mount_path(value):
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


# Default streaming/copy chunk size. Flat 16 kB on every platform: the 20 ms loop-slice budget
# (see the buffer-usage plan) caps a main-loop chunk near 16 kB even on the fastest S3 SD path,
# so a larger loop chunk is unsafe. The platform distinction lives one level down, in the C++
# allocator (alloc_dma_capable): on the worker task -- which has no 20 ms budget -- S3/P4 stage a
# 32 kB chunk in DMA-capable PSRAM, while every loop-path buffer stays 16 kB internal. An
# explicit copy_chunk_size still overrides this default (the user's last word). Multiple of 512
# to keep FATFS whole-sector transfers.
_DEFAULT_COPY_CHUNK_SIZE = 16384


# Fallback when no driver reported anything (storage configured without a device).
# Mirrors STORAGE_PATH_MAX's compile-time fallback in storage.h.
_DEFAULT_PATH_MAX = 256

# The copy walk's two path buffers are allocated once and shared by every level (see
# append_path_segment in storage.cpp), so they are a flat cost. What scales with depth is the
# walk's own frame plus the driver's list_dir()/remove() frames, where the FATFS LFN buffers
# dominate. Keep 25% of the task stack free for whatever called into the walk.
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


def _resolve_path_max(config) -> int:
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
            bounds.append(_FATFS_SHORT_NAME_MAX)
            return max(bounds)
        lfn = opts.get("CONFIG_FATFS_MAX_LFN", _FATFS_MAX_LFN_DEFAULT)
        # A YAML sdkconfig_options entry arrives wrapped so it is written out verbatim; the
        # esp32 component's own default is a plain int. Both carry the same number.
        lfn = getattr(lfn, "value", lfn)
        try:
            # A name plus its terminator is the longest single component FATFS will hand back.
            bounds.append(int(lfn) + 1)
        except (TypeError, ValueError):
            _LOGGER.warning(
                "storage: CONFIG_FATFS_MAX_LFN is %r, which is not a number -- using %d for the "
                "path bound instead",
                lfn,
                _DEFAULT_PATH_MAX,
            )
            bounds.append(_DEFAULT_PATH_MAX)
    return max(bounds) if bounds else _DEFAULT_PATH_MAX


def _default_copy_chunk_size() -> int:
    """The loop-safe base chunk size (platform-independent -- see the note above)."""
    return _DEFAULT_COPY_CHUNK_SIZE


# storage is a dependency of every driver and would otherwise run BEFORE them (default
# priority), reading device_count/worker_count as 0 -- every driver's own to_code() is where
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
    # Compile-time bound for the enumeration snapshot in StorageRegistry::for_each*.
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

        # The tree walks run on the worker task when one is configured; a path bound raised
        # past what its stack can carry would overflow rather than fail cleanly.
        if _get_data().worker_count > 0:
            needed = _walk_stack_bytes(path_max, _MAX_RECURSION_DEPTH)
            budget = int(config[CONF_TASK_STACK_SIZE] * _WALK_STACK_HEADROOM)
            if needed > budget:
                _LOGGER.warning(
                    "storage: a %d-level tree walk with path_max %d needs roughly %d bytes of "
                    "stack, leaving little headroom in task_stack_size (%d). Raise "
                    "task_stack_size if deep copy/remove operations misbehave.",
                    _MAX_RECURSION_DEPTH,
                    path_max,
                    needed,
                    config[CONF_TASK_STACK_SIZE],
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

        worker_id = ID(f"{var}_worker", is_declaration=True, type=StorageWorker)
        CORE.component_ids.add(str(worker_id))
        worker_var = cg.new_Pvariable(worker_id)
        await cg.register_component(worker_var, {})

        cg.add(worker_var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))
        cg.add(worker_var.set_task_priority(config[CONF_TASK_PRIORITY]))
        cg.add(worker_var.set_max_pending(config[CONF_MAX_PENDING]))
        cg.add(worker_var.set_max_streams(config[CONF_MAX_STREAMS]))
        cg.add(worker_var.set_update_interval(config[CONF_WORKER_UPDATE_INTERVAL]))

        cg.add(cg.RawExpression(f"{storage_ns}::global_storage_worker = {worker_var}"))


# ---------------------------------------------------------------------------
# Globally available file-op actions: storage.file_write / file_append / file_read.
# Like web_server sorting groups, these work everywhere once storage is loaded
# (every storage driver AUTO_LOADs it) -- no per-component preparation required.
# ---------------------------------------------------------------------------

CONF_CONTENT = "content"
CONF_NEWLINE = "newline"
CONF_EXTRACT = "extract"
CONF_JSON = "json"
CONF_TO_GLOBAL = "to_global"
CONF_LINE = "line"
CONF_SPLIT = "split"
CONF_SEPARATOR = "separator"
CONF_REGEX = "regex"
CONF_TRIM = "trim"

FileWriteAction = storage_ns.class_("FileWriteAction", automation.Action)
FileReadAction = storage_ns.class_("FileReadAction", automation.Action)
RawReadAction = storage_ns.class_("RawReadAction", automation.Action)
RawWriteAction = storage_ns.class_("RawWriteAction", automation.Action)
RawEraseAction = storage_ns.class_("RawEraseAction", automation.Action)
ExtractStepType = storage_ns.enum("ExtractStepType", is_class=True)


def _validate_write_content(config):
    has_content = CONF_CONTENT in config
    has_format = CONF_FORMAT in config
    if has_content == has_format:
        raise cv.Invalid("Exactly one of 'content' or 'format' is required")
    if config.get(CONF_ARGS) and not has_format:
        raise cv.Invalid("'args' requires 'format'")
    return config


def _file_write_schema(newline_default):
    return cv.All(
        cv.Schema(
            {
                cv.Required(CONF_PATH): cv.templatable(cv.string),
                cv.Optional(CONF_CONTENT): cv.templatable(cv.string),
                cv.Optional(CONF_FORMAT): cv.string,
                cv.Optional(CONF_ARGS, default=[]): cv.ensure_list(cv.lambda_),
                cv.Optional(CONF_NEWLINE, default=newline_default): cv.boolean,
            }
        ),
        _validate_write_content,
    )


def _validate_regex(value):
    value = cv.string(value)
    try:
        # Python's re syntax is a close superset of std::regex ECMAScript for the
        # constructs typically used here; this catches plain syntax errors at
        # config time so the (exception-free) runtime never sees a bad pattern.
        re.compile(value)
    except re.error as e:
        raise cv.Invalid(f"Invalid regex: {e}") from e
    return value


def _exactly_one_step_kind(config):
    kinds = [
        k
        for k in (CONF_LINE, CONF_SPLIT, CONF_KEY, CONF_REGEX, CONF_TRIM, CONF_JSON)
        if k in config
    ]
    if len(kinds) != 1:
        raise cv.Invalid(
            f"Each extract step needs exactly one of line/split/key/regex/trim/json, got {kinds}"
        )
    if CONF_INDEX in config and CONF_SPLIT not in config:
        raise cv.Invalid("'index' is only valid with 'split'")
    if CONF_SEPARATOR in config and CONF_KEY not in config:
        raise cv.Invalid("'separator' is only valid with 'key'")
    if CONF_GROUP in config and CONF_REGEX not in config:
        raise cv.Invalid("'group' is only valid with 'regex'")
    return config


_EXTRACT_STEP_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_LINE): cv.positive_not_null_int,
            # '/'-separated pointer into a JSON document ("a/b/0").
            cv.Optional(CONF_JSON): cv.string_strict,
            # Non-empty: an empty separator makes the split loop spin without advancing and
            # hand back the whole buffer, and an empty key matches every line -- both are
            # silently useless rather than wrong, which is worse to debug than a rejection.
            cv.Optional(CONF_SPLIT): cv.All(cv.string_strict, cv.Length(min=1)),
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_KEY): cv.All(cv.string_strict, cv.Length(min=1)),
            cv.Optional(CONF_SEPARATOR): cv.All(cv.string_strict, cv.Length(min=1)),
            cv.Optional(CONF_REGEX): _validate_regex,
            cv.Optional(CONF_GROUP): cv.positive_int,
            cv.Optional(CONF_TRIM): cv.boolean,
        }
    ),
    _exactly_one_step_kind,
)


def _validate_read(config):
    if CONF_TO_GLOBAL not in config and CONF_ON_VALUE not in config:
        raise cv.Invalid("At least one of 'to_global' or 'on_value' is required")
    return config


_FILE_READ_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_PATH): cv.templatable(cv.string),
            cv.Optional(CONF_EXTRACT, default=[]): cv.ensure_list(_EXTRACT_STEP_SCHEMA),
            cv.Optional(CONF_TO_GLOBAL): cv.use_id(globals_.GlobalsComponent),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(single=True),
        }
    ),
    _validate_read,
)


async def _build_write_action(config, action_id, template_arg, args, append):
    var = cg.new_Pvariable(action_id, template_arg, append)
    template_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(template_))
    if CONF_CONTENT in config:
        template_ = await cg.templatable(config[CONF_CONTENT], args, cg.std_string)
        cg.add(var.set_content(template_))
    else:
        # Render printf-style format + args into the content string, logger.log-style:
        # the validated arg lambdas are embedded verbatim as C++ expressions.
        format_literal = str(cg.safe_exp(config[CONF_FORMAT]))
        # each arg is normalized through printf_arg(): std::string -> c_str(),
        # everything else passes through (see automation.h for the UB rationale)
        arg_exprs = "".join(
            f", esphome::storage::printf_arg({x})" for x in config[CONF_ARGS]
        )
        lambda_ = await cg.process_lambda(
            core.Lambda(f"return str_sprintf({format_literal}{arg_exprs});"),
            args,
            return_type=cg.std_string,
        )
        cg.add(var.set_content(lambda_))
    cg.add(var.set_newline(config[CONF_NEWLINE]))
    return var


@automation.register_action(
    "storage.file_write",
    FileWriteAction,
    _file_write_schema(newline_default=False),
    synchronous=True,
)
async def file_write_action_to_code(config, action_id, template_arg, args):
    return await _build_write_action(config, action_id, template_arg, args, False)


@automation.register_action(
    "storage.file_append",
    FileWriteAction,
    _file_write_schema(newline_default=True),
    synchronous=True,
)
async def file_append_action_to_code(config, action_id, template_arg, args):
    return await _build_write_action(config, action_id, template_arg, args, True)


@automation.register_action(
    "storage.file_read",
    FileReadAction,
    _FILE_READ_SCHEMA,
    synchronous=True,
)
async def file_read_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    template_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
    cg.add(var.set_path(template_))

    for step in config[CONF_EXTRACT]:
        if CONF_LINE in step:
            cg.add(var.add_step(ExtractStepType.LINE, "", "", step[CONF_LINE]))
        elif CONF_SPLIT in step:
            cg.add(
                var.add_step(
                    ExtractStepType.SPLIT, step[CONF_SPLIT], "", step.get(CONF_INDEX, 0)
                )
            )
        elif CONF_KEY in step:
            cg.add(
                var.add_step(
                    ExtractStepType.KEY,
                    step[CONF_KEY],
                    step.get(CONF_SEPARATOR, "="),
                    0,
                )
            )
        elif CONF_JSON in step:
            cg.add_define("USE_STORAGE_JSON_EXTRACT")
            cg.add(var.add_step(ExtractStepType.JSON, step[CONF_JSON], "", 0))
        elif CONF_REGEX in step:
            cg.add_define("USE_STORAGE_REGEX_EXTRACT")
            # group default: 1 (first capture) if the pattern has groups, else whole match
            default_group = 1 if re.compile(step[CONF_REGEX]).groups > 0 else 0
            cg.add(
                var.add_step(
                    ExtractStepType.REGEX,
                    step[CONF_REGEX],
                    "",
                    step.get(CONF_GROUP, default_group),
                )
            )
        elif CONF_TRIM in step and step[CONF_TRIM]:
            cg.add(var.add_step(ExtractStepType.TRIM, "", "", 0))

    if CONF_TO_GLOBAL in config:
        glob = await cg.get_variable(config[CONF_TO_GLOBAL])
        cg.add(
            var.set_global_setter(
                cg.RawExpression(
                    f"[](const std::string &x) {{ {storage_ns}::assign_from_string({glob}, x); }}"
                )
            )
        )
    if CONF_ON_VALUE in config:
        await automation.build_automation(
            var.get_value_trigger(), [(cg.std_string, "x")], config[CONF_ON_VALUE]
        )
    return var


CONF_RECURSIVE = "recursive"

FileCopyAction = storage_ns.class_("FileCopyAction", automation.Action)
FileDeleteAction = storage_ns.class_("FileDeleteAction", automation.Action)
FileExistsCondition = storage_ns.class_("FileExistsCondition", automation.Condition)

_FILE_COPY_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_FROM): cv.templatable(cv.string),
        cv.Required(CONF_TO): cv.templatable(cv.string),
        # Fired from the worker's completion callback (main loop). `x` is the error text,
        # empty string on success. The copy/move runs asynchronously -- the action sequence
        # does not wait for it.
        cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
    }
)


async def _build_copy_action(config, action_id, template_arg, args, is_move):
    # file_copy/move prefer the async worker (see perform_file_copy_async). We deliberately do
    # NOT request_storage_worker() here: action codegen can run after the storage to_code has
    # already snapshotted the worker count (LATE), so a late request would compile the action
    # against a worker that was never created. Instead the C++ side degrades cleanly -- if the
    # worker isn't compiled in (no path driver requested it), it runs the blocking helper. Any
    # node that can actually do file ops has a path driver, which requests the worker anyway.
    var = cg.new_Pvariable(action_id, template_arg, is_move)
    cg.add(var.set_from(await cg.templatable(config[CONF_FROM], args, cg.std_string)))
    cg.add(var.set_to(await cg.templatable(config[CONF_TO], args, cg.std_string)))
    if CONF_ON_COMPLETE in config:
        await automation.build_automation(
            var.get_complete_trigger(), [(cg.std_string, "x")], config[CONF_ON_COMPLETE]
        )
    return var


@automation.register_action(
    "storage.file_copy", FileCopyAction, _FILE_COPY_SCHEMA, synchronous=True
)
async def file_copy_action_to_code(config, action_id, template_arg, args):
    return await _build_copy_action(config, action_id, template_arg, args, False)


# Doubles as a rename action: same-storage moves take the rename() fast path internally.
@automation.register_action(
    "storage.file_move", FileCopyAction, _FILE_COPY_SCHEMA, synchronous=True
)
async def file_move_action_to_code(config, action_id, template_arg, args):
    return await _build_copy_action(config, action_id, template_arg, args, True)


@automation.register_action(
    "storage.file_delete",
    FileDeleteAction,
    cv.Schema(
        {
            cv.Required(CONF_PATH): cv.templatable(cv.string),
            cv.Optional(CONF_RECURSIVE, default=False): cv.boolean,
        }
    ),
    synchronous=True,
)
async def file_delete_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add(var.set_path(await cg.templatable(config[CONF_PATH], args, cg.std_string)))
    cg.add(var.set_recursive(config[CONF_RECURSIVE]))
    return var


@automation.register_condition(
    "storage.file_exists",
    FileExistsCondition,
    cv.maybe_simple_value(
        {cv.Required(CONF_PATH): cv.templatable(cv.string)}, key=CONF_PATH
    ),
)
async def file_exists_condition_to_code(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    cg.add(var.set_path(await cg.templatable(config[CONF_PATH], args, cg.std_string)))
    return var


MountAction = storage_ns.class_("MountAction", automation.Action)

_MOUNT_SCHEMA = cv.maybe_simple_value(
    {cv.Required(CONF_ID): cv.use_id(MountableStorage)}, key=CONF_ID
)


async def _build_mount_action(config, action_id, template_arg, args, mount):
    # cv.use_id(MountableStorage) already rejected non-removable targets at YAML time.
    target = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, target, mount)


@automation.register_action(
    "storage.mount", MountAction, _MOUNT_SCHEMA, synchronous=True
)
async def mount_action_to_code(config, action_id, template_arg, args):
    return await _build_mount_action(config, action_id, template_arg, args, True)


@automation.register_action(
    "storage.unmount", MountAction, _MOUNT_SCHEMA, synchronous=True
)
async def unmount_action_to_code(config, action_id, template_arg, args):
    return await _build_mount_action(config, action_id, template_arg, args, False)


# ---------------------------------------------------------------------------
# storage.raw_read / storage.raw_write / storage.raw_erase
# ---------------------------------------------------------------------------
# Address-based access to a RawStorage device. Ranges and capabilities are the device's own
# answer at runtime (RawGeometry/RawEraseCaps) -- codegen only wires the parameters through.

CONF_TO_FILE = "to_file"
CONF_FROM_FILE = "from_file"
CONF_ERASE_FIRST = "erase_first"
CONF_FORCE_SLICED_ERASE = "force_sliced_erase"


def _validate_raw_data(value):
    if isinstance(value, str):
        return value.encode("utf-8")
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid(
        "data must either be a string wrapped in quotes or a list of bytes"
    )


def _validate_raw_read(config):
    if CONF_TO_FILE not in config and CONF_ON_VALUE not in config:
        raise cv.Invalid("At least one of 'to_file' or 'on_value' is required")
    if CONF_SIZE not in config and CONF_TO_FILE not in config:
        raise cv.Invalid("'size' is required unless reading into a file with 'to_file'")
    return config


def _validate_raw_write(config):
    if (CONF_DATA in config) == (CONF_FROM_FILE in config):
        raise cv.Invalid("Exactly one of 'data' or 'from_file' is required")
    return config


def _validate_raw_erase(config):
    if config[CONF_ALL]:
        if CONF_ADDRESS in config or CONF_SIZE in config:
            raise cv.Invalid("'all' erases the whole device -- remove 'address'/'size'")
    elif CONF_SIZE not in config:
        raise cv.Invalid(
            "'size' is required unless erasing the whole device with 'all'"
        )
    return config


_RAW_READ_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RawStorage),
            cv.Optional(CONF_ADDRESS): cv.templatable(cv.hex_uint32_t),
            # Omitted with to_file: read to the end of the device (0 = rest, see the action).
            cv.Optional(CONF_SIZE): cv.templatable(cv.positive_int),
            cv.Optional(CONF_TO_FILE): cv.templatable(cv.string),
            cv.Optional(CONF_ON_VALUE): automation.validate_automation(single=True),
            # Fires (error text, empty = success) when a to_file read lands on the worker.
            cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
        }
    ),
    _validate_raw_read,
)

_RAW_WRITE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RawStorage),
            cv.Optional(CONF_ADDRESS): cv.templatable(cv.hex_uint32_t),
            cv.Optional(CONF_DATA): cv.templatable(_validate_raw_data),
            cv.Optional(CONF_FROM_FILE): cv.templatable(cv.string),
            # Media reporting RAW_WRITE_NEEDS_ERASE (NOR flash) need the covering sectors erased
            # first -- which also wipes whatever else shares them, hence opt-in.
            cv.Optional(CONF_ERASE_FIRST, default=False): cv.boolean,
            # Fires (error text, empty = success) when a from_file write lands on the worker.
            cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
        }
    ),
    _validate_raw_write,
)

_RAW_ERASE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(RawStorage),
            cv.Optional(CONF_ADDRESS): cv.templatable(cv.hex_uint32_t),
            cv.Optional(CONF_SIZE): cv.templatable(cv.positive_int),
            cv.Optional(CONF_ALL, default=False): cv.boolean,
            # Opt out of the whole-chip fast path: force the block-by-block erase even where a
            # single chip erase would be used (task-safe device, full span). Default keeps the
            # fast path. Mainly a testing/benchmarking knob.
            cv.Optional(CONF_FORCE_SLICED_ERASE, default=False): cv.boolean,
            # Fires (error text, empty = success) when the erase finishes on the worker.
            cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
        }
    ),
    _validate_raw_erase,
)


@automation.register_action(
    "storage.raw_read", RawReadAction, _RAW_READ_SCHEMA, synchronous=True
)
async def raw_read_action_to_code(config, action_id, template_arg, args):
    cg.add_define("USE_STORAGE_RAW_ACTIONS")
    device = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, device)
    cg.add(
        var.set_address(
            await cg.templatable(config.get(CONF_ADDRESS, 0), args, cg.uint32)
        )
    )
    cg.add(
        var.set_size(await cg.templatable(config.get(CONF_SIZE, 0), args, cg.uint32))
    )
    if CONF_TO_FILE in config:
        cg.add(
            var.set_to_file(
                await cg.templatable(config[CONF_TO_FILE], args, cg.std_string)
            )
        )
        cg.add(var.set_has_to_file(True))
    if CONF_ON_VALUE in config:
        await automation.build_automation(
            var.get_value_trigger(),
            [(cg.std_vector.template(cg.uint8), "x")],
            config[CONF_ON_VALUE],
        )
    if CONF_ON_COMPLETE in config:
        await automation.build_automation(
            var.get_complete_trigger(), [(cg.std_string, "x")], config[CONF_ON_COMPLETE]
        )
    return var


@automation.register_action(
    "storage.raw_write", RawWriteAction, _RAW_WRITE_SCHEMA, synchronous=True
)
async def raw_write_action_to_code(config, action_id, template_arg, args):
    cg.add_define("USE_STORAGE_RAW_ACTIONS")
    device = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, device)
    cg.add(
        var.set_address(
            await cg.templatable(config.get(CONF_ADDRESS, 0), args, cg.uint32)
        )
    )
    cg.add(var.set_erase_first(config[CONF_ERASE_FIRST]))
    if CONF_FROM_FILE in config:
        cg.add(
            var.set_from_file(
                await cg.templatable(config[CONF_FROM_FILE], args, cg.std_string)
            )
        )
        cg.add(var.set_has_from_file(True))
    if CONF_ON_COMPLETE in config:
        await automation.build_automation(
            var.get_complete_trigger(), [(cg.std_string, "x")], config[CONF_ON_COMPLETE]
        )
        return var
    data = config[CONF_DATA]
    if isinstance(data, bytes):
        data = list(data)
    if cg.is_template(data):
        templ = await cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        # Static payload stays in flash -- no RAM copy (same as uart.write).
        arr_id = ID(f"{action_id}_data", is_declaration=True, type=cg.uint8)
        arr = cg.static_const_array(arr_id, cg.ArrayInitializer(*data))
        cg.add(var.set_data_static(arr, len(data)))
    return var


@automation.register_action(
    "storage.raw_erase", RawEraseAction, _RAW_ERASE_SCHEMA, synchronous=True
)
async def raw_erase_action_to_code(config, action_id, template_arg, args):
    cg.add_define("USE_STORAGE_RAW_ACTIONS")
    device = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, device)
    cg.add(
        var.set_address(
            await cg.templatable(config.get(CONF_ADDRESS, 0), args, cg.uint32)
        )
    )
    cg.add(
        var.set_size(await cg.templatable(config.get(CONF_SIZE, 0), args, cg.uint32))
    )
    cg.add(var.set_all(config[CONF_ALL]))
    cg.add(var.set_force_sliced_erase(config[CONF_FORCE_SLICED_ERASE]))
    if CONF_ON_COMPLETE in config:
        await automation.build_automation(
            var.get_complete_trigger(), [(cg.std_string, "x")], config[CONF_ON_COMPLETE]
        )
    return var
