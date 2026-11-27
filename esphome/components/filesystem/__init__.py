from dataclasses import dataclass, field

import esphome.codegen as cg
from esphome.components.storage import CONF_STORAGE_ID, BlockDevice
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, ID, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.cpp_generator import MockObj, MockObjClass
from esphome.types import ConfigType, SafeExpType

CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["storage"]
DOMAIN = "filesystem"
IS_PLATFORM_COMPONENT = True

CONF_FILESYSTEM_ID = "filesystem_id"
CONF_FORMAT_IF_MOUNT_FAILED = "format_if_mount_failed"
CONF_MAX_OPEN_DIRS = "max_open_dirs"
CONF_MAX_OPEN_FILES = "max_open_files"

MAX_OPEN_FILES = 32
MAX_OPEN_DIRS = 8

filesystem_ns = cg.esphome_ns.namespace("filesystem")
Filesystem = filesystem_ns.class_("Filesystem", cg.Component)

FILESYSTEM_ID_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FILESYSTEM_ID): cv.use_id(Filesystem),
    }
)


@dataclass
class _Pools:
    var: MockObj
    files: int
    dirs: int


@dataclass
class FilesystemData:
    pools: dict[str, _Pools] = field(default_factory=dict)
    requests: dict[str, list[int]] = field(default_factory=dict)
    job_added: bool = False


def _get_data() -> FilesystemData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = FilesystemData()
    return CORE.data[DOMAIN]


def validate_path(value: object) -> str:
    """Validate an absolute path on a filesystem."""
    value = cv.string_strict(value)
    if not value.startswith("/"):
        raise cv.Invalid("The path must start with '/'")
    return value


def request_open_handles(filesystem_id: ID, files: int = 1, dirs: int = 0) -> None:
    """Reserve file and directory handles that a consumer keeps open at the same time.

    Call it from to_code. The reserved handles are added to the filesystem's own max_open_files and
    max_open_dirs.
    """
    counts = _get_data().requests.setdefault(str(filesystem_id), [0, 0])
    counts[0] += files
    counts[1] += dirs


def filesystem_schema(class_: MockObjClass) -> cv.Schema:
    """The base schema for a filesystem platform; `extend` it with the platform's own options."""
    return cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(class_),
            cv.GenerateID(CONF_STORAGE_ID): cv.use_id(BlockDevice),
            cv.Optional(CONF_MAX_OPEN_FILES, default=2): cv.int_range(
                min=0, max=MAX_OPEN_FILES
            ),
            cv.Optional(CONF_MAX_OPEN_DIRS, default=1): cv.int_range(
                min=0, max=MAX_OPEN_DIRS
            ),
            cv.Optional(CONF_FORMAT_IF_MOUNT_FAILED, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(CoroPriority.FINAL)
async def _emit_pool_sizes() -> None:
    data = _get_data()
    for key, pools in data.pools.items():
        files, dirs = data.requests.get(key, (0, 0))
        files += pools.files
        dirs += pools.dirs
        if files > MAX_OPEN_FILES or dirs > MAX_OPEN_DIRS:
            raise EsphomeError(
                f"Filesystem '{key}' needs {files} open files and {dirs} open directories, counting those "
                f"its users reserve; the limits are {MAX_OPEN_FILES} and {MAX_OPEN_DIRS}. Lower "
                f"{CONF_MAX_OPEN_FILES} or {CONF_MAX_OPEN_DIRS}"
            )
        cg.add(pools.var.set_pool_sizes(files, dirs))


async def new_filesystem(config: ConfigType, *args: SafeExpType) -> MockObj:
    """Create a filesystem platform component on its block device.

    The platform registers it with cg.register_component(), which names the component after the module that calls it.
    """
    device = await cg.get_variable(config[CONF_STORAGE_ID])
    var = cg.new_Pvariable(config[CONF_ID], device, *args)
    if config[CONF_FORMAT_IF_MOUNT_FAILED]:
        cg.add(var.set_format_if_mount_failed(True))
    data = _get_data()
    data.pools[str(config[CONF_ID])] = _Pools(
        var, config[CONF_MAX_OPEN_FILES], config[CONF_MAX_OPEN_DIRS]
    )
    if not data.job_added:
        data.job_added = True
        CORE.add_job(_emit_pool_sizes)
    return var


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_FILESYSTEM")
