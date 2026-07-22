from dataclasses import dataclass, field
import hashlib
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
    CONF_DEVICE,
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
TransferBuffer = storage_ns.class_("TransferBuffer", cg.Component)
StoragePtr = Storage.operator("ptr")
PathStorage = storage_ns.class_("PathStorage", Storage)
RawStorage = storage_ns.class_("RawStorage", Storage)
MountableStorage = storage_ns.class_("MountableStorage")
StorageRegistry = storage_ns.class_("StorageRegistry", cg.Component)
StorageWorker = storage_ns.class_("StorageWorker", cg.PollingComponent)


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
        cv.Optional("enable_psram_transfer_buffer"): cv.boolean,
        cv.Optional("psram_transfer_buffer_size"): cv.All(
            cv.validate_bytes, cv.Range(min=64 * 1024)
        ),
        cv.Optional("psram_transfer_buffer_override_limit", default=False): cv.boolean,
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
        # Fixed request pool/queue depth — sized exactly at codegen like the storage
        # registry's device count, no heap allocation per request at runtime.
        cv.Optional(CONF_MAX_PENDING, default=4): cv.int_range(min=1, max=16),
        # Fixed stream pool depth (begin_write()/begin_read() and friends, storage_worker.h) —
        # streams are typically much longer-lived than a single copy/move (e.g. one HTTP
        # upload in progress), so a node doing one at a time needs very few slots.
        cv.Optional(CONF_MAX_STREAMS, default=2): cv.int_range(min=1, max=8),
        # How often the async worker's engine runs (PollingComponent update_interval). The
        # worker is driven by this scheduler interval, not the gated component loop(); a small
        # value keeps chunked transfers moving at a good rate without busy-spinning the CPU.
        cv.Optional(
            CONF_WORKER_UPDATE_INTERVAL, default="5ms"
        ): cv.positive_time_period_milliseconds,
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
    # Largest path length any configured driver reported via request_path_length().
    path_max: int = 0
    # Set by FATFS-backed drivers, whose bound is not a constant but whatever
    # CONFIG_FATFS_MAX_LFN ends up being — see request_fatfs_path_length().
    fatfs_path_bound: bool = False
    worker_count: int = 0
    worker_task_safe: bool = False
    # Raw preference regions per device id: every export/import action's address, plus the
    # container size when it can be computed. Filled while the actions are built and resolved
    # once at the end — see _resolve_raw_pref_regions().
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
    internal FixedVector is sized exactly — no compile-time upper bound needed.
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


def _transfer_buffer_final_validate(config):
    has_psram = "psram" in fv.full_config.get()
    if "enable_psram_transfer_buffer" in config and not has_psram:
        raise cv.Invalid(
            "'enable_psram_transfer_buffer' is only available with the psram component"
        )
    enabled = config.get("enable_psram_transfer_buffer", has_psram)
    if "psram_transfer_buffer_size" in config:
        if not has_psram:
            raise cv.Invalid(
                "'psram_transfer_buffer_size' is only available with the psram component"
            )
        if not enabled:
            raise cv.Invalid(
                "'psram_transfer_buffer_size' requires 'enable_psram_transfer_buffer' to be true"
            )
    if config.get("psram_transfer_buffer_override_limit") and (
        not has_psram or not enabled
    ):
        raise cv.Invalid(
            "'psram_transfer_buffer_override_limit' requires an enabled psram transfer buffer"
        )
    # The 25% default and the 80% ceiling are enforced in setup(): the actual PSRAM
    # size is detected at boot and unknowable at config time.
    return config


FINAL_VALIDATE_SCHEMA = _transfer_buffer_final_validate


# Default streaming/copy chunk size. Flat 16 kB on every platform: the 20 ms loop-slice budget
# (see the buffer-usage plan) caps a main-loop chunk near 16 kB even on the fastest S3 SD path,
# so a larger loop chunk is unsafe. The platform distinction lives one level down, in the C++
# allocator (alloc_dma_capable): on the worker task — which has no 20 ms budget — S3/P4 stage a
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
    # did (storage configured without a device), not a floor under the derivation — used as
    # one it would swallow a lowered CONFIG_FATFS_MAX_LFN and make this whole resolution moot.
    bounds = []
    if data.path_max > 0:
        bounds.append(data.path_max)
    if data.fatfs_path_bound and CORE.is_esp32:
        from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS

        opts = CORE.data.get(KEY_ESP32, {}).get(KEY_SDKCONFIG_OPTIONS, {})
        lfn = opts.get("CONFIG_FATFS_MAX_LFN", _FATFS_MAX_LFN_DEFAULT)
        # A YAML sdkconfig_options entry arrives wrapped so it is written out verbatim; the
        # esp32 component's own default is a plain int. Both carry the same number.
        lfn = getattr(lfn, "value", lfn)
        try:
            # A name plus its terminator is the longest single component FATFS will hand back.
            bounds.append(int(lfn) + 1)
        except (TypeError, ValueError):
            _LOGGER.warning(
                "storage: CONFIG_FATFS_MAX_LFN is %r, which is not a number — using %d for the "
                "path bound instead",
                lfn,
                _DEFAULT_PATH_MAX,
            )
            bounds.append(_DEFAULT_PATH_MAX)
    return max(bounds) if bounds else _DEFAULT_PATH_MAX


def _default_copy_chunk_size() -> int:
    """The loop-safe base chunk size (platform-independent — see the note above)."""
    return _DEFAULT_COPY_CHUNK_SIZE


# storage is a dependency of every driver and would otherwise run BEFORE them (default
# priority), reading device_count/worker_count as 0 — every driver's own to_code() is where
# request_storage_device()/request_storage_worker() actually get called. LATE (-100) runs
# after all default-priority driver to_code()s, so those counts are final by the time this
# reads them. Consumers awaiting the registry/worker variables (e.g. via cg.get_variable())
# are unaffected either way, since that call already suspends until the variable exists.
@coroutine_with_priority(CoroPriority.LATE)
async def to_code(config):
    tb_enabled = config.get("enable_psram_transfer_buffer", "psram" in CORE.config)
    if tb_enabled:
        cg.add_define("USE_STORAGE_TRANSFER_BUFFER")
        tb_id = ID("storage_transfer_buffer", is_declaration=True, type=TransferBuffer)
        CORE.component_ids.add(str(tb_id))
        tb = cg.new_Pvariable(tb_id)
        await cg.register_component(tb, {})
        # 0 = auto: setup() sizes the arena to 25% of the detected PSRAM
        cg.add(tb.set_size(config.get("psram_transfer_buffer_size", 0)))
        cg.add(tb.set_override_limit(config["psram_transfer_buffer_override_limit"]))
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
# (every storage driver AUTO_LOADs it) — no per-component preparation required.
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
            cv.Optional(CONF_SPLIT): cv.string_strict,
            cv.Optional(CONF_INDEX): cv.positive_int,
            cv.Optional(CONF_KEY): cv.string_strict,
            cv.Optional(CONF_SEPARATOR): cv.string_strict,
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
        # empty string on success. The copy/move runs asynchronously — the action sequence
        # does not wait for it.
        cv.Optional(CONF_ON_COMPLETE): automation.validate_automation(single=True),
    }
)


async def _build_copy_action(config, action_id, template_arg, args, is_move):
    # file_copy/move prefer the async worker (see perform_file_copy_async). We deliberately do
    # NOT request_storage_worker() here: action codegen can run after the storage to_code has
    # already snapshotted the worker count (LATE), so a late request would compile the action
    # against a worker that was never created. Instead the C++ side degrades cleanly — if the
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


# ==================== PREFERENCES BACKUP/RESTORE ====================
# ---------------------------------------------------------------------------
# storage.raw_read / storage.raw_write / storage.raw_erase
# ---------------------------------------------------------------------------
# Address-based access to a RawStorage device. Ranges and capabilities are the device's own
# answer at runtime (RawGeometry/RawEraseCaps) — codegen only wires the parameters through.

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
            raise cv.Invalid("'all' erases the whole device — remove 'address'/'size'")
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
            # first — which also wipes whatever else shares them, hence opt-in.
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
        # Static payload stays in flash — no RAM copy (same as uart.write).
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


# storage.export_preferences / storage.import_preferences: back up ESPHome
# preferences (the "esphome" NVS namespace) to a storage target and restore
# them. Provided by storage itself, guard-protected: the C++ only compiles
# in when one of these actions is used, and only on esp32 (preferences are
# always NVS-backed there — no extra YAML needed to "enable" them).
#
# Selection is an option on the action: `preferences:` lists global IDs —
# with it, only those entries round-trip and export under their YAML id;
# without it, the whole namespace round-trips under numeric NVS keys.

CONF_PREFERENCES = "preferences"
CONF_REBOOT = "reboot"

# Keep in sync with globals_component.h ("1944399030U ^ this->name_hash_")
# and the md5-based name hash in globals/__init__.py.
_GLOBALS_KEY_XOR = 1944399030

# YAML `type:` -> (PrefType tag, element size irrelevant here). Blob layouts:
# scalars/arrays are raw T bytes; std::string is length-prefixed char[SZ]
# with SZ = max_restore_data_length (default 63) + 1 — keep in sync with
# globals/__init__.py.
_PREF_SCALAR_TYPES = {
    "bool": "BOOL",
    "int8_t": "I8",
    "char": "I8",
    "uint8_t": "U8",
    "unsigned char": "U8",
    "int16_t": "I16",
    "short": "I16",
    "uint16_t": "U16",
    "unsigned short": "U16",
    "int": "I32",
    "int32_t": "I32",
    "long": "I32",
    "uint32_t": "U32",
    "unsigned int": "U32",
    "unsigned long": "U32",
    "size_t": "U32",
    "float": "F32",
    "double": "F64",
}
_ARRAY_TYPE_RE = re.compile(r"^\s*(.+?)\s*\[\s*(\d+)\s*\]\s*$")

_RESTORING_RE = re.compile(r"RestoringGlobalsComponent<\s*(.+?)\s*>\s*$")
_RESTORING_STRING_RE = re.compile(
    r"RestoringGlobalStringComponent<\s*.+?,\s*(\d+)\s*>\s*$"
)


# Sensor platforms whose restore type codegen can name but the runtime sweep cannot: they all
# arrive as sensor::Sensor in App's list, four bytes wide, and a build without RTTI cannot tell
# them apart. The platform is right there in the YAML, so map the registered class to the kind
# and let register_entity_pref() carry it over. A platform that is not listed here keeps the
# sweep's RAW entry -- named, hex value -- which is the safe fallback: a wrong kind would render
# a wrong number AND write wrong bytes back on import.
_SENSOR_PREF_KINDS = {
    "total_daily_energy::TotalDailyEnergy": "FLOAT",
    "integration::IntegrationSensor": "FLOAT",
    "duty_time_sensor::DutyTimeSensor": "U32",
    "rotary_encoder::RotaryEncoderSensor": "I32",
}


# Preferences owned by a component rather than an entity: (component, C++ symbol, exported
# name, kind). Emitted only when that component is configured, and by SYMBOL -- the value stays
# in the owning component's header, so a rename breaks the build instead of silently exporting a
# number that has moved on. Without this they show up as a bare key, since the sweep only walks
# entities.
_COMPONENT_PREF_KEYS = (("safe_mode", "safe_mode::RTC_KEY", "safe_mode", "U32"),)


async def _register_component_prefs():
    """Names the component-owned preferences whose owners are part of this build."""
    for component, symbol, name, kind in _COMPONENT_PREF_KEYS:
        if component not in CORE.config:
            continue
        cg.add(
            cg.RawExpression(
                f'{storage_ns}::register_key_pref({symbol}, "{name}", '
                f"{storage_ns}::EntityKind::{kind})"
            )
        )


async def _register_typed_sensors():
    """Emits one register_entity_pref() per sensor whose restore type is known.

    FINAL priority: every sensor platform's own to_code() must have registered its variable
    before CORE.variables can be walked for them.
    """
    for reg_id in CORE.variables:
        # Registered types carry no esphome:: prefix; tolerate one anyway.
        type_str = str(reg_id.type).removeprefix("esphome::")
        kind = _SENSOR_PREF_KINDS.get(type_str)
        if kind is None:
            continue
        var = await cg.get_variable(reg_id)
        cg.add(
            cg.RawExpression(
                f"{storage_ns}::register_entity_pref({var}, "
                f"{storage_ns}::EntityKind::{kind})"
            )
        )


def _pref_type_from_class(type_str: str) -> tuple[str, int] | None:
    """(PrefType tag, count) from a declared global's C++ class string —
    codegen-world data only (ID.type of the registered variable). None when
    the class is not a restoring global at all."""
    if m := _RESTORING_STRING_RE.search(type_str):
        return "STRING", int(m.group(1))  # SZ straight from the template arg
    if m := _RESTORING_RE.search(type_str):
        inner = m.group(1)
        if inner in _PREF_SCALAR_TYPES:
            return _PREF_SCALAR_TYPES[inner], 1
        if am := _ARRAY_TYPE_RE.match(inner):
            base, count = am.group(1), int(am.group(2))
            if base in _PREF_SCALAR_TYPES:
                return _PREF_SCALAR_TYPES[base], count
        return "HEX", 0  # restoring, but a type we cannot render — hex round-trip
    return None


ExportPreferencesAction = storage_ns.class_(
    "ExportPreferencesAction", automation.Action
)
ImportPreferencesAction = storage_ns.class_(
    "ImportPreferencesAction", automation.Action
)


def _global_nvs_key(global_id: str) -> int:
    name_hash = int(hashlib.md5(global_id.encode()).hexdigest()[:8], 16)
    return (_GLOBALS_KEY_XOR ^ name_hash) & 0xFFFFFFFF


_PREFERENCES_ACTION_BASE = {
    cv.Optional(CONF_PATH): cv.templatable(cv.string_strict),
    # No default: the presence of the key is what distinguishes a file target from a raw one
    # (a default would fill it in and make that check meaningless).
    cv.Optional(CONF_FORMAT): cv.one_of("kv", "json", lower=True),
    cv.Optional(CONF_DEVICE): cv.use_id(RawStorage),
    # Not templatable on purpose: codegen computes each region's room from these addresses,
    # which a runtime lambda would hide.
    cv.Optional(CONF_ADDRESS): cv.hex_uint32_t,
    # Globals with restore_value. cv.use_id(cg.Component) because the globals
    # component declares several unrelated classes (GlobalsComponent,
    # RestoringGlobalsComponent, RestoringGlobalStringComponent) — only the
    # id string is consumed here (baked into the name<->key table), the
    # variable itself is never awaited.
    cv.Optional(CONF_PREFERENCES): cv.ensure_list(cv.use_id(cg.Component)),
}


def _validate_preferences_target(config):
    has_path = CONF_PATH in config
    has_device = CONF_DEVICE in config
    if has_path == has_device:
        raise cv.Invalid("Exactly one of 'path' or 'device' is required")
    if has_device:
        if CONF_FORMAT in config:
            raise cv.Invalid(
                "'format' does not apply to a raw device: the blob is written as stored, "
                "there is nothing to render"
            )
        if CONF_ADDRESS not in config:
            raise cv.Invalid("'address' is required when the target is a raw device")
    elif CONF_ADDRESS in config:
        raise cv.Invalid("'address' only applies to a raw device target ('device:')")
    return config


_EXPORT_PREFERENCES_SCHEMA = cv.All(
    cv.only_on(["esp32"]),
    cv.Schema(_PREFERENCES_ACTION_BASE),
    _validate_preferences_target,
)

_IMPORT_PREFERENCES_SCHEMA = cv.All(
    cv.only_on(["esp32"]),
    cv.Schema(
        {
            **_PREFERENCES_ACTION_BASE,
            # Preferences are read at boot — imported values only take effect
            # after a restart. Opt-in convenience.
            cv.Optional(CONF_REBOOT, default=False): cv.boolean,
        }
    ),
    _validate_preferences_target,
)


# Per-type version constants of EntityBase::make_entity_preference_() callers.
# Keep in sync: fan/fan.cpp, climate/climate.cpp; every other core entity uses
# the default version 0. template text is special-cased (trait-salted key).
# (module, class, version, EntityKind) — kinds map to real-struct codecs in
# preferences_backup.cpp; anything not matched below registers as RAW (named,
# hex value). datetime template platforms carry their own versions.
# Container arithmetic, used to catch overlapping regions at config time. Layout is fixed by
# preferences_backup.cpp: a 16-byte header plus {key u32, len u16, blob} per entry.
_RAW_PREF_HEADER = 16
_RAW_PREF_ENTRY_OVERHEAD = 6
_PREF_TYPE_SIZES = {
    "BOOL": 1,
    "I8": 1,
    "U8": 1,
    "I16": 2,
    "U16": 2,
    "I32": 4,
    "U32": 4,
    "F32": 4,
    "F64": 8,
}


def _raw_pref_size(entries: list[tuple[str, int]]) -> int:
    """Exact container size for an explicit selection: (PrefType tag, count) per entry."""
    total = _RAW_PREF_HEADER
    for tag, count in entries:
        # STRING blobs are length-prefixed char[SZ], with SZ already carried in count.
        elem = 1 if tag == "STRING" else _PREF_TYPE_SIZES[tag]
        total += _RAW_PREF_ENTRY_OVERHEAD + elem * count
    return total


async def _resolve_raw_pref_regions():
    """Hands every raw preferences action the room it actually has, and rejects regions that
    would run into each other.

    Codegen knows every action's address, so nobody has to repeat "and it may use N bytes":
    a region reaches up to the next address on the same device, and the last one to the end of
    the device — which only the device knows, hence window 0 for it. An export and its import
    share one address by design (that is the pair), so actions are grouped by address, not
    counted individually.

    Where a selection is explicit the container size is exact and a collision is a config
    error. An unrestricted selection grows with the app, so that case cannot be sized here and
    is caught at runtime by the window instead — the export refuses rather than writing into
    the neighbouring region."""
    for device, actions in _get_data().raw_pref_regions.items():
        by_address: dict[int, dict] = {}
        for action in actions:
            region = by_address.setdefault(
                action["address"], {"size": None, "actions": []}
            )
            region["actions"].append(action)
            if action["size"] is not None:
                region["size"] = max(region["size"] or 0, action["size"])

        addresses = sorted(by_address)
        for i, address in enumerate(addresses):
            region = by_address[address]
            if i + 1 < len(addresses):
                window = addresses[i + 1] - address
                size = region["size"]
                if size is not None and size > window:
                    raise cv.Invalid(
                        f"The preferences region at 0x{address:X} on '{device}' needs {size} "
                        f"bytes and would run into the region at 0x{addresses[i + 1]:X} "
                        f"({window} bytes apart)"
                    )
            else:
                window = 0  # to the end of the device
            for action in region["actions"]:
                cg.add(action["var"].set_raw_target(action["device"], address, window))


def _register_raw_pref_region(device_id, device_var, address, size, var):
    data = _get_data()
    data.raw_pref_regions.setdefault(str(device_id), []).append(
        {"address": address, "size": size, "var": var, "device": device_var}
    )
    if not data.raw_pref_job_queued:
        data.raw_pref_job_queued = True
        # FINAL: every action must be built before the regions can be laid out.
        CORE.add_job(
            coroutine_with_priority(CoroPriority.FINAL)(_resolve_raw_pref_regions)
        )


async def _build_preferences_action(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    cg.add_define("USE_STORAGE_PREFERENCES")
    # Once per build, not per action: naming is a property of the node, not of the action.
    data = _get_data()
    if not data.sensor_pref_job_queued:
        data.sensor_pref_job_queued = True
        CORE.add_job(
            coroutine_with_priority(CoroPriority.FINAL)(_register_typed_sensors)
        )
        CORE.add_job(
            coroutine_with_priority(CoroPriority.FINAL)(_register_component_prefs)
        )
    if CONF_PATH in config:
        template_ = await cg.templatable(config[CONF_PATH], args, cg.std_string)
        cg.add(var.set_path(template_))
        cg.add(var.set_format(config.get(CONF_FORMAT, "kv")))

    def _bake(entries, restrict):
        # entity-only selections produce zero table entries: emitting
        # "static const T x[] = {}" would be a zero-size array (GNU
        # extension, not ISO C++) — pass a null table instead
        if not entries:
            cg.add(var.set_selection(cg.nullptr, 0, restrict))
            return

        arr = f"{action_id}_psel"
        cg.add_global(
            cg.RawExpression(
                f"static const esphome::storage::PrefSelection {arr}[] = {{"
                + ", ".join(entries)
                + "}"
            )
        )
        cg.add(var.set_selection(cg.RawExpression(arr), len(entries), restrict))

    def _entry(name, tag, count):
        key = _global_nvs_key(name)
        return f'{{"{name}", {key}UL, esphome::storage::PrefType::{tag}, {count}}}'

    if selection := config.get(CONF_PREFERENCES):
        # get_variable_with_full_id is a coroutine: it suspends until the
        # global's own to_code has registered the variable — the declaration
        # ID it returns carries the real C++ class (codegen-world data, no
        # validation-step leftovers).
        entries = []
        sizes = []
        has_entities = False
        for gid in selection:
            full_id, obj = await cg.get_variable_with_full_id(gid)
            parsed = _pref_type_from_class(str(full_id.type))
            if parsed is not None:
                entries.append(_entry(gid.id, *parsed))
                sizes.append(parsed)
                continue
            # anything else is treated as an entity: the runtime sweep
            # resolves name/kind/key from the live object; unresolvable
            # selections log a loud skip at play time
            cg.add(var.add_selected_entity(obj))
            has_entities = True
        if entries or has_entities:
            _bake(entries, True)
        # Entity selections carry no codegen-known blob size (their layout is a component
        # private, resolved by the runtime sweep) — the size stays unknown then, and only the
        # window guards that case.
        raw_size = None if has_entities else _raw_pref_size(sizes)
    else:
        # All mode: enumerate the codegen variable registry once every
        # pending to_code has run. Scheduled as its own coroutine job — it is
        # enqueued behind all already-queued component jobs, so the globals
        # are registered by the time it executes.
        # globals' own to_code runs at CoroPriority.LATE (-100) — an
        # unprioritized job would enumerate CORE.variables BEFORE any global
        # is registered (verified empirically: 14 vars, zero globals).
        # FINAL (-1000) queues the bake after every component job.
        @coroutine_with_priority(CoroPriority.FINAL)
        async def _bake_all():
            # globals only — entity naming is entirely the runtime sweep's job
            entries = []
            for reg_id in CORE.variables:
                parsed = _pref_type_from_class(str(reg_id.type))
                if parsed is not None:
                    entries.append(_entry(reg_id.id, *parsed))
            if entries:
                _bake(entries, False)

        CORE.add_job(_bake_all)
        raw_size = (
            None  # the namespace grows with the app — only the window can guard this
        )

    if CONF_DEVICE in config:
        device_var = await cg.get_variable(config[CONF_DEVICE])
        _register_raw_pref_region(
            config[CONF_DEVICE], device_var, config[CONF_ADDRESS], raw_size, var
        )
    return var


@automation.register_action(
    "storage.export_preferences",
    ExportPreferencesAction,
    _EXPORT_PREFERENCES_SCHEMA,
    synchronous=True,
)
async def export_preferences_to_code(config, action_id, template_arg, args):
    return await _build_preferences_action(config, action_id, template_arg, args)


@automation.register_action(
    "storage.import_preferences",
    ImportPreferencesAction,
    _IMPORT_PREFERENCES_SCHEMA,
    synchronous=True,
)
async def import_preferences_to_code(config, action_id, template_arg, args):
    var = await _build_preferences_action(config, action_id, template_arg, args)
    cg.add(var.set_reboot(config[CONF_REBOOT]))
    return var


# ---- file_system option (sd_storage / usb_storage) --------------------------------------
# The option does not exist without esp32 enable_exfat: without exFAT the filesystem is
# always FAT32, there is nothing to choose and nothing to probe — the mount path stays
# exactly as it is today. With exFAT enabled the option appears with default "auto"
# (FatFs' own boot-sector detection inside f_mount); an explicit fat32/exfat probes the
# medium BEFORE the mount and reformats first on mismatch, so the one mount that happens
# is already on the requested filesystem.
CONF_FILE_SYSTEM = "file_system"
FILE_SYSTEM_AUTO = "auto"
FILE_SYSTEM_FAT32 = "fat32"
FILE_SYSTEM_EXFAT = "exfat"

FILE_SYSTEM_SCHEMA_ENTRY = cv.Optional(CONF_FILE_SYSTEM)
validate_file_system_value = cv.one_of(
    FILE_SYSTEM_AUTO, FILE_SYSTEM_FAT32, FILE_SYSTEM_EXFAT, lower=True
)


def _esp32_exfat_enabled(fconf) -> bool:
    from esphome.components.esp32 import CONF_ENABLE_EXFAT
    from esphome.components.esp32.const import KEY_ESP32
    from esphome.const import CONF_ADVANCED, CONF_FRAMEWORK

    esp32 = fconf.get(KEY_ESP32)
    if not esp32:
        return False
    return bool(
        esp32.get(CONF_FRAMEWORK, {})
        .get(CONF_ADVANCED, {})
        .get(CONF_ENABLE_EXFAT, False)
    )


def final_validate_file_system(config) -> None:
    """Reject the option outright when exFAT is not compiled in."""
    if CONF_FILE_SYSTEM not in config:
        return
    if not _esp32_exfat_enabled(fv.full_config.get()):
        raise cv.Invalid(
            f"'{CONF_FILE_SYSTEM}' is not available without 'enable_exfat: true' in the "
            f"esp32 framework advanced options — without exFAT the filesystem is always "
            f"FAT32 and there is nothing to choose"
        )


async def file_system_to_code(var, config) -> None:
    """Emit the selection define + setter — only when the option may exist at all."""
    from esphome.core import CORE

    if not _esp32_exfat_enabled(CORE.config):
        return  # not even the auto path is compiled in
    import esphome.codegen as cg

    cg.add_define("USE_STORAGE_FILE_SYSTEM_SELECT")
    fs = config.get(CONF_FILE_SYSTEM, FILE_SYSTEM_AUTO)
    value = {FILE_SYSTEM_AUTO: 0, FILE_SYSTEM_FAT32: 1, FILE_SYSTEM_EXFAT: 2}[fs]
    cg.add(var.set_requested_file_system(value))
