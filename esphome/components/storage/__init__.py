from dataclasses import dataclass
import logging

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE, ID, CoroPriority, coroutine_with_priority
import esphome.final_validate as fv

CODEOWNERS = ["@p1ngb4ck"]

_LOGGER = logging.getLogger(__name__)

DOMAIN = "storage"

CONF_COPY_CHUNK_SIZE = "copy_chunk_size"
CONF_PATH_MAX = "path_max"
CONF_MAX_BLOCKING_TRANSFER_SIZE = "max_blocking_transfer_size"
CONF_MOVE_FALLBACK_COPY = "move_fallback_copy"
CONF_ON_COMPLETE = "on_complete"

# Not yet in esphome/const.py
CONF_ON_REGISTERED = "on_registered"
CONF_ON_UNREGISTERED = "on_unregistered"


storage_ns = cg.esphome_ns.namespace("storage")
Storage = storage_ns.class_("Storage", cg.Component)
TransferBuffer = storage_ns.class_("TransferBuffer", cg.Component)
StoragePtr = Storage.operator("ptr")
PathStorage = storage_ns.class_("PathStorage", Storage)
RawStorage = storage_ns.class_("RawStorage", Storage)
MountableStorage = storage_ns.class_("MountableStorage")
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
    # did (storage configured without a device), not a floor under the derivation — used as
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
# priority), reading device_count as 0 — every driver's own to_code() is where
# request_storage_device() actually gets called. LATE (-100) runs
# after all default-priority driver to_code()s, so those counts are final by the time this
# reads them. Consumers awaiting the registry variable (e.g. via cg.get_variable())
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
