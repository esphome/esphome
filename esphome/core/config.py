from __future__ import annotations

import logging
import os
from pathlib import Path

from esphome import automation, core
import esphome.codegen as cg
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA,
    CONF_AREA_ID,
    CONF_AREAS,
    CONF_BUILD_PATH,
    CONF_COMMENT,
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_DEBUG_SCHEDULER,
    CONF_DEVICES,
    CONF_ENVIRONMENT_VARIABLES,
    CONF_ESPHOME,
    CONF_FRIENDLY_NAME,
    CONF_ID,
    CONF_INCLUDES,
    CONF_INCLUDES_C,
    CONF_LIBRARIES,
    CONF_MIN_VERSION,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    CONF_ON_BOOT,
    CONF_ON_LOOP,
    CONF_ON_SHUTDOWN,
    CONF_ON_UPDATE,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PRIORITY,
    CONF_PROJECT,
    CONF_TRIGGER_ID,
    CONF_VERSION,
    KEY_CORE,
    PlatformFramework,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import (
    CORE,
    KEY_CONTROLLER_REGISTRY_COUNT,
    CoroPriority,
    coroutine_with_priority,
)
from esphome.helpers import (
    copy_file_if_changed,
    fnv1a_32bit_hash,
    get_str_env,
    walk_files,
)
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

StartupTrigger = cg.esphome_ns.class_(
    "StartupTrigger", cg.Component, automation.Trigger.template()
)
ShutdownTrigger = cg.esphome_ns.class_(
    "ShutdownTrigger", cg.Component, automation.Trigger.template()
)
LoopTrigger = cg.esphome_ns.class_(
    "LoopTrigger", cg.Component, automation.Trigger.template()
)
ProjectUpdateTrigger = cg.esphome_ns.class_(
    "ProjectUpdateTrigger", cg.Component, automation.Trigger.template(cg.std_string)
)
Device = cg.esphome_ns.class_("Device")
Area = cg.esphome_ns.class_("Area")

VALID_INCLUDE_EXTS = {".h", ".hpp", ".tcc", ".ino", ".cpp", ".c"}


def validate_hostname(config):
    max_length = 31
    if config[CONF_NAME_ADD_MAC_SUFFIX]:
        max_length -= 7  # "-AABBCC" is appended when add mac suffix option is used
    if len(config[CONF_NAME]) > max_length:
        raise cv.Invalid(
            f"Hostnames can only be {max_length} characters long", path=[CONF_NAME]
        )
    if "_" in config[CONF_NAME]:
        _LOGGER.warning(
            "'%s': Using the '_' (underscore) character in the hostname is discouraged "
            "as it can cause problems with some DHCP and local name services. "
            "For more information, see https://esphome.io/guides/faq/#why-shouldnt-i-use-underscores-in-my-device-name",
            config[CONF_NAME],
        )
    return config


def validate_ids_and_references(config: ConfigType) -> ConfigType:
    """Validate that there are no hash collisions between IDs and that area_id references are valid.

    This validation is critical because we use 32-bit hashes for performance on microcontrollers.
    By detecting collisions at compile time, we prevent any runtime issues while maintaining
    optimal performance on 32-bit platforms. In practice, with typical deployments having only
    a handful of areas and devices, hash collisions are virtually impossible.
    """

    # Helper to check hash collisions
    def check_hash_collision(
        id_obj: core.ID,
        hash_dict: dict[int, str],
        item_type: str,
        path: list[str | int],
    ) -> None:
        hash_val: int = fnv1a_32bit_hash(id_obj.id)
        if hash_val in hash_dict and hash_dict[hash_val] != id_obj.id:
            raise cv.Invalid(
                f"{item_type} ID '{id_obj.id}' with hash {hash_val} collides with "
                f"existing {item_type.lower()} ID '{hash_dict[hash_val]}'",
                path=path,
            )
        hash_dict[hash_val] = id_obj.id

    # Collect all areas
    all_areas: list[dict[str, str | core.ID]] = []
    if CONF_AREA in config:
        all_areas.append(config[CONF_AREA])
    all_areas.extend(config[CONF_AREAS])

    # Validate area hash collisions and collect IDs
    area_hashes: dict[int, str] = {}
    area_ids: set[str] = set()
    for area in all_areas:
        area_id: core.ID = area[CONF_ID]
        check_hash_collision(area_id, area_hashes, "Area", [CONF_AREAS, area_id.id])
        area_ids.add(area_id.id)

    # Validate device hash collisions and area references
    device_hashes: dict[int, str] = {}
    for device in config[CONF_DEVICES]:
        device_id: core.ID = device[CONF_ID]
        check_hash_collision(
            device_id, device_hashes, "Device", [CONF_DEVICES, device_id.id]
        )

    return config


def valid_include(value: str) -> str:
    # Look for "<...>" includes
    if value.startswith("<") and value.endswith(">"):
        return value
    try:
        return str(cv.directory(value))
    except cv.Invalid:
        pass
    path = cv.file_(value)
    ext = path.suffix
    if ext not in VALID_INCLUDE_EXTS:
        raise cv.Invalid(
            f"Include has invalid file extension {ext} - valid extensions are {', '.join(VALID_INCLUDE_EXTS)}"
        )
    return str(path)


def valid_project_name(value: str):
    if value.count(".") != 1:
        raise cv.Invalid("project name needs to have a namespace")
    return value


def get_usable_cpu_count() -> int:
    """Return the number of CPUs that can be used for processes.
    On Python 3.13+ this is the number of CPUs that can be used for processes.
    On older Python versions this is the number of CPUs.
    """
    return (
        os.process_cpu_count() if hasattr(os, "process_cpu_count") else os.cpu_count()
    )


if "ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT" in os.environ:
    _compile_process_limit_default = min(
        int(os.environ["ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT"]), get_usable_cpu_count()
    )
else:
    _compile_process_limit_default = cv.UNDEFINED

AREA_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Area),
        cv.Required(CONF_NAME): cv.string,
    }
)

DEVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(Device),
        cv.Required(CONF_NAME): cv.string,
        cv.Optional(CONF_AREA_ID): cv.use_id(Area),
    }
)


def validate_area_config(config: dict | str) -> dict[str, str | core.ID]:
    return cv.maybe_simple_value(AREA_SCHEMA, key=CONF_NAME)(config)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.valid_name,
            cv.Optional(CONF_FRIENDLY_NAME, ""): cv.All(cv.string, cv.Length(max=120)),
            cv.Optional(CONF_AREA): validate_area_config,
            cv.Optional(CONF_COMMENT): cv.string,
            cv.Required(CONF_BUILD_PATH): cv.string,
            cv.Optional(CONF_PLATFORMIO_OPTIONS, default={}): cv.Schema(
                {
                    cv.string_strict: cv.Any([cv.string], cv.string),
                }
            ),
            cv.Optional(CONF_ENVIRONMENT_VARIABLES, default={}): cv.Schema(
                {
                    cv.string_strict: cv.string,
                }
            ),
            cv.Optional(CONF_ON_BOOT): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(StartupTrigger),
                    cv.Optional(CONF_PRIORITY, default=600.0): cv.float_,
                }
            ),
            cv.Optional(CONF_ON_SHUTDOWN): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ShutdownTrigger),
                    cv.Optional(CONF_PRIORITY, default=600.0): cv.float_,
                }
            ),
            cv.Optional(CONF_ON_LOOP): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(LoopTrigger),
                }
            ),
            cv.Optional(CONF_INCLUDES, default=[]): cv.ensure_list(valid_include),
            cv.Optional(CONF_INCLUDES_C, default=[]): cv.ensure_list(valid_include),
            cv.Optional(CONF_LIBRARIES, default=[]): cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_NAME_ADD_MAC_SUFFIX, default=False): cv.boolean,
            cv.Optional(CONF_DEBUG_SCHEDULER, default=False): cv.boolean,
            cv.Optional(CONF_PROJECT): cv.Schema(
                {
                    cv.Required(CONF_NAME): cv.All(
                        cv.string_strict, valid_project_name
                    ),
                    cv.Required(CONF_VERSION): cv.string_strict,
                    cv.Optional(CONF_ON_UPDATE): automation.validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                                ProjectUpdateTrigger
                            ),
                        }
                    ),
                }
            ),
            cv.Optional(CONF_MIN_VERSION, default=ESPHOME_VERSION): cv.All(
                cv.version_number, cv.validate_esphome_version
            ),
            cv.Optional(
                CONF_COMPILE_PROCESS_LIMIT, default=_compile_process_limit_default
            ): cv.int_range(min=1, max=get_usable_cpu_count()),
            cv.Optional(CONF_AREAS, default=[]): cv.ensure_list(AREA_SCHEMA),
            cv.Optional(CONF_DEVICES, default=[]): cv.ensure_list(DEVICE_SCHEMA),
        }
    ),
    validate_hostname,
)


FINAL_VALIDATE_SCHEMA = cv.All(validate_ids_and_references)


PRELOAD_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.valid_name,
        cv.Optional(CONF_BUILD_PATH): cv.string,
        cv.Optional(CONF_PLATFORM): cv.invalid(
            "Please remove the `platform` key from the [esphome] block and use the correct platform component. This style of configuration has now been removed."
        ),
        cv.Optional(CONF_MIN_VERSION, default=ESPHOME_VERSION): cv.All(
            cv.version_number, cv.validate_esphome_version
        ),
    },
    extra=cv.ALLOW_EXTRA,
)


def _is_target_platform(name):
    from esphome.loader import get_component

    try:
        return get_component(name, True).is_target_platform
    except KeyError:
        pass
    except ImportError:
        pass
    return False


def _list_target_platforms():
    target_platforms = []
    root = Path(__file__).parents[1]
    for path in (root / "components").iterdir():
        if not path.is_dir():
            continue
        if not (path / "__init__.py").is_file():
            continue
        if _is_target_platform(path.name):
            target_platforms += [path.name]
    return target_platforms


def _sort_includes_by_type(includes: list[str]) -> tuple[list[str], list[str]]:
    system_includes = []
    other_includes = []
    for include in includes:
        if include.startswith("<") and include.endswith(">"):
            system_includes.append(include)
        else:
            other_includes.append(include)
    return system_includes, other_includes


def preload_core_config(config, result) -> str:
    with cv.prepend_path(CONF_ESPHOME):
        conf = PRELOAD_CONFIG_SCHEMA(config[CONF_ESPHOME])

    CORE.name = conf[CONF_NAME]
    CORE.friendly_name = conf.get(CONF_FRIENDLY_NAME)
    CORE.data[KEY_CORE] = {}

    if CONF_BUILD_PATH not in conf:
        build_path = Path(get_str_env("ESPHOME_BUILD_PATH", "build"))
        conf[CONF_BUILD_PATH] = str(build_path / CORE.name)
    CORE.build_path = CORE.data_dir / conf[CONF_BUILD_PATH]

    target_platforms = []

    for domain in config:
        if domain.startswith("."):
            continue
        if _is_target_platform(domain):
            target_platforms += [domain]

    if not target_platforms:
        raise cv.Invalid(
            "Platform missing. You must include one of the available platform keys: "
            + ", ".join(_list_target_platforms()),
            [CONF_ESPHOME],
        )
    if len(target_platforms) > 1:
        raise cv.Invalid(
            f"Found multiple target platform blocks: {', '.join(target_platforms)}. Only one is allowed.",
            [target_platforms[0]],
        )

    config[CONF_ESPHOME] = conf
    return target_platforms[0]


def include_file(path: Path, basename: Path, is_c_header: bool = False):
    parts = basename.parts
    dst = CORE.relative_src_path(*parts)
    copy_file_if_changed(path, dst)

    ext = path.suffix
    if ext in [".h", ".hpp", ".tcc"]:
        # Header, add include statement
        if is_c_header:
            # Wrap in extern "C" block for C headers
            cg.add_global(
                cg.RawStatement(f'extern "C" {{\n  #include "{basename}"\n}}')
            )
        else:
            # Regular include
            cg.add_global(cg.RawStatement(f'#include "{basename}"'))


ARDUINO_GLUE_CODE = """\
#define yield() esphome::yield()
#define millis() esphome::millis()
#define micros() esphome::micros()
#define delay(x) esphome::delay(x)
#define delayMicroseconds(x) esphome::delayMicroseconds(x)
"""


@coroutine_with_priority(CoroPriority.WORKAROUNDS)
async def add_arduino_global_workaround():
    # The Arduino framework defined these itself in the global
    # namespace. For the esphome codebase that is not a problem,
    # but when custom code
    #   1. writes `millis()` for example AND
    #   2. has `using namespace esphome;` like our guides suggest
    # Then the compiler will complain that the call is ambiguous
    # Define a hacky macro so that the call is never ambiguous
    # and always uses the esphome namespace one.
    # See also https://github.com/esphome/issues/issues/2510
    # Priority -999 so that it runs before adding includes, as those
    # also might reference these symbols
    for line in ARDUINO_GLUE_CODE.splitlines():
        cg.add_global(cg.RawStatement(line))


@coroutine_with_priority(CoroPriority.FINAL)
async def add_includes(includes: list[str], is_c_header: bool = False) -> None:
    # Add includes at the very end, so that the included files can access global variables
    for include in includes:
        path = CORE.relative_config_path(include)
        if path.is_dir():
            # Directory, copy tree
            for p in walk_files(path):
                basename = p.relative_to(path.parent)
                include_file(p, basename, is_c_header)
        else:
            # Copy file
            basename = Path(path.name)
            include_file(path, basename, is_c_header)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_platformio_options(pio_options):
    # Add includes at the very end, so that they override everything
    for key, val in pio_options.items():
        if key in ["build_flags", "lib_ignore"] and not isinstance(val, list):
            val = [val]
        cg.add_platformio_option(key, val)


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_environment_variables(env_vars: dict[str, str]) -> None:
    # Set environment variables for the build process
    os.environ.update(env_vars)


@coroutine_with_priority(CoroPriority.AUTOMATION)
async def _add_automations(config):
    for conf in config.get(CONF_ON_BOOT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf.get(CONF_PRIORITY))
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SHUTDOWN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf.get(CONF_PRIORITY))
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_LOOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)


# Datetime component has special subtypes that need additional defines
DATETIME_SUBTYPES = {"date", "time", "datetime"}


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_platform_defines() -> None:
    # Generate compile-time defines for platforms that have actual entities
    # Only add USE_* and count defines when there are entities
    for platform_name, count in sorted(CORE.platform_counts.items()):
        if count <= 0:
            continue

        define_name = f"ESPHOME_ENTITY_{platform_name.upper()}_COUNT"
        cg.add_define(define_name, count)

        # Datetime subtypes only use USE_DATETIME_* defines
        if platform_name in DATETIME_SUBTYPES:
            cg.add_define(f"USE_DATETIME_{platform_name.upper()}")
        else:
            # Regular platforms use USE_* defines
            cg.add_define(f"USE_{platform_name.upper()}")


@coroutine_with_priority(CoroPriority.FINAL)
async def _add_controller_registry_define() -> None:
    # Generate StaticVector size for ControllerRegistry
    controller_count = CORE.data.get(KEY_CONTROLLER_REGISTRY_COUNT, 0)
    if controller_count > 0:
        cg.add_define("USE_CONTROLLER_REGISTRY")
        cg.add_define("CONTROLLER_REGISTRY_MAX", controller_count)


@coroutine_with_priority(CoroPriority.CORE)
async def to_code(config: ConfigType) -> None:
    cg.add_global(cg.global_ns.namespace("esphome").using)
    # These can be used by user lambdas, put them to default scope
    cg.add_global(cg.RawExpression("using std::isnan"))
    cg.add_global(cg.RawExpression("using std::min"))
    cg.add_global(cg.RawExpression("using std::max"))

    cg.add(
        cg.App.pre_setup(
            config[CONF_NAME],
            config[CONF_FRIENDLY_NAME],
            config.get(CONF_COMMENT, ""),
            cg.RawExpression('__DATE__ ", " __TIME__'),
            config[CONF_NAME_ADD_MAC_SUFFIX],
        )
    )
    # Define component count for static allocation
    cg.add_define("ESPHOME_COMPONENT_COUNT", len(CORE.component_ids))

    CORE.add_job(_add_platform_defines)
    CORE.add_job(_add_controller_registry_define)

    CORE.add_job(_add_automations, config)

    cg.add_build_flag("-fno-exceptions")

    # Libraries
    for lib in config[CONF_LIBRARIES]:
        if "@" in lib:
            name, vers = lib.split("@", 1)
            cg.add_library(name, vers)
        elif "://" in lib:
            # Repository...
            if "=" in lib:
                name, repo = lib.split("=", 1)
                cg.add_library(name, None, repo)
            else:
                cg.add_library(None, None, lib)

        else:
            cg.add_library(lib, None)

    cg.add_build_flag("-Wno-unused-variable")
    cg.add_build_flag("-Wno-unused-but-set-variable")
    cg.add_build_flag("-Wno-sign-compare")
    if config[CONF_DEBUG_SCHEDULER]:
        cg.add_define("ESPHOME_DEBUG_SCHEDULER")

    if CORE.using_arduino and not CORE.is_bk72xx:
        CORE.add_job(add_arduino_global_workaround)

    if config[CONF_INCLUDES]:
        system_includes, other_includes = _sort_includes_by_type(config[CONF_INCLUDES])
        # <...> includes should be at the start
        for include in system_includes:
            cg.add_global(cg.RawStatement(f"#include {include}"), prepend=True)
        # Other includes should be at the end
        CORE.add_job(add_includes, other_includes, False)

    if config[CONF_INCLUDES_C]:
        system_includes, other_includes = _sort_includes_by_type(
            config[CONF_INCLUDES_C]
        )
        # <...> includes should be at the start
        for include in system_includes:
            cg.add_global(
                cg.RawStatement(f'extern "C" {{\n  #include {include}\n}}'),
                prepend=True,
            )
        # Other includes should be at the end
        CORE.add_job(add_includes, other_includes, True)

    if project_conf := config.get(CONF_PROJECT):
        cg.add_define("ESPHOME_PROJECT_NAME", project_conf[CONF_NAME])
        cg.add_define("ESPHOME_PROJECT_VERSION", project_conf[CONF_VERSION])
        cg.add_define("ESPHOME_PROJECT_VERSION_30", project_conf[CONF_VERSION][:29])
        for conf in project_conf.get(CONF_ON_UPDATE, []):
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
            await cg.register_component(trigger, conf)
            await automation.build_automation(
                trigger, [(cg.std_string, "version")], conf
            )

    if config[CONF_PLATFORMIO_OPTIONS]:
        CORE.add_job(_add_platformio_options, config[CONF_PLATFORMIO_OPTIONS])

    if config[CONF_ENVIRONMENT_VARIABLES]:
        CORE.add_job(_add_environment_variables, config[CONF_ENVIRONMENT_VARIABLES])

    # Process areas
    all_areas: list[dict[str, str | core.ID]] = []
    if CONF_AREA in config:
        all_areas.append(config[CONF_AREA])
    all_areas.extend(config[CONF_AREAS])

    if all_areas:
        cg.add_define("USE_AREAS")
        cg.add_define("ESPHOME_AREA_COUNT", len(all_areas))

        for area_conf in all_areas:
            area_id: core.ID = area_conf[CONF_ID]
            area_id_hash: int = fnv1a_32bit_hash(area_id.id)
            area_name: str = area_conf[CONF_NAME]

            area_var = cg.new_Pvariable(area_id)
            cg.add(area_var.set_area_id(area_id_hash))
            cg.add(area_var.set_name(area_name))
            cg.add(cg.App.register_area(area_var))

    # Process devices
    devices: list[dict[str, str | core.ID]] = config[CONF_DEVICES]
    if not devices:
        return

    # Define device count for static allocation
    cg.add_define("USE_DEVICES")
    cg.add_define("ESPHOME_DEVICE_COUNT", len(devices))

    # Process each device
    for dev_conf in devices:
        device_id: core.ID = dev_conf[CONF_ID]
        device_id_hash = fnv1a_32bit_hash(device_id.id)
        device_name: str = dev_conf[CONF_NAME]

        dev = cg.new_Pvariable(device_id)
        cg.add(dev.set_device_id(device_id_hash))
        cg.add(dev.set_name(device_name))

        # Set area if specified
        if CONF_AREA_ID in dev_conf:
            area_id: core.ID = dev_conf[CONF_AREA_ID]
            area_id_hash = fnv1a_32bit_hash(area_id.id)
            cg.add(dev.set_area_id(area_id_hash))

        cg.add(cg.App.register_device(dev))


# Platform-specific source files for core
FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "ring_buffer.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        # Note: lock_free_queue.h and event_pool.h are header files and don't need to be filtered
        # as they are only included when needed by the preprocessor
    }
)
