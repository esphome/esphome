import logging
import multiprocessing
import os
from pathlib import Path

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA,
    CONF_BUILD_PATH,
    CONF_COMMENT,
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    CONF_FRIENDLY_NAME,
    CONF_INCLUDES,
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
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, get_str_env, walk_files

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
            "For more information, see https://esphome.io/guides/faq.html#why-shouldn-t-i-use-underscores-in-my-device-name",
            config[CONF_NAME],
        )
    return config


def valid_include(value):
    # Look for "<...>" includes
    if value.startswith("<") and value.endswith(">"):
        return value
    try:
        return cv.directory(value)
    except cv.Invalid:
        pass
    value = cv.file_(value)
    _, ext = os.path.splitext(value)
    if ext not in VALID_INCLUDE_EXTS:
        raise cv.Invalid(
            f"Include has invalid file extension {ext} - valid extensions are {', '.join(VALID_INCLUDE_EXTS)}"
        )
    return value


def valid_project_name(value: str):
    if value.count(".") != 1:
        raise cv.Invalid("project name needs to have a namespace")
    return value


if "ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT" in os.environ:
    _compile_process_limit_default = min(
        int(os.environ["ESPHOME_DEFAULT_COMPILE_PROCESS_LIMIT"]),
        multiprocessing.cpu_count(),
    )
else:
    _compile_process_limit_default = cv.UNDEFINED


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_NAME): cv.valid_name,
            cv.Optional(CONF_FRIENDLY_NAME, ""): cv.string,
            cv.Optional(CONF_AREA, ""): cv.string,
            cv.Optional(CONF_COMMENT): cv.string,
            cv.Required(CONF_BUILD_PATH): cv.string,
            cv.Optional(CONF_PLATFORMIO_OPTIONS, default={}): cv.Schema(
                {
                    cv.string_strict: cv.Any([cv.string], cv.string),
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
            cv.Optional(CONF_LIBRARIES, default=[]): cv.ensure_list(cv.string_strict),
            cv.Optional(CONF_NAME_ADD_MAC_SUFFIX, default=False): cv.boolean,
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
            ): cv.int_range(min=1, max=multiprocessing.cpu_count()),
        }
    ),
    validate_hostname,
)

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
        if get_component(name, True).is_target_platform:
            return True
    except KeyError:
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


def preload_core_config(config, result) -> str:
    with cv.prepend_path(CONF_ESPHOME):
        conf = PRELOAD_CONFIG_SCHEMA(config[CONF_ESPHOME])

    CORE.name = conf[CONF_NAME]
    CORE.friendly_name = conf.get(CONF_FRIENDLY_NAME)
    CORE.data[KEY_CORE] = {}

    if CONF_BUILD_PATH not in conf:
        build_path = get_str_env("ESPHOME_BUILD_PATH", "build")
        conf[CONF_BUILD_PATH] = os.path.join(build_path, CORE.name)
    CORE.build_path = CORE.relative_internal_path(conf[CONF_BUILD_PATH])

    target_platforms = []

    for domain, _ in config.items():
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


def include_file(path, basename):
    parts = basename.split(os.path.sep)
    dst = CORE.relative_src_path(*parts)
    copy_file_if_changed(path, dst)

    _, ext = os.path.splitext(path)
    if ext in [".h", ".hpp", ".tcc"]:
        # Header, add include statement
        cg.add_global(cg.RawStatement(f'#include "{basename}"'))


ARDUINO_GLUE_CODE = """\
#define yield() esphome::yield()
#define millis() esphome::millis()
#define micros() esphome::micros()
#define delay(x) esphome::delay(x)
#define delayMicroseconds(x) esphome::delayMicroseconds(x)
"""


@coroutine_with_priority(-999.0)
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


@coroutine_with_priority(-1000.0)
async def add_includes(includes):
    # Add includes at the very end, so that the included files can access global variables
    for include in includes:
        path = CORE.relative_config_path(include)
        if os.path.isdir(path):
            # Directory, copy tree
            for p in walk_files(path):
                basename = os.path.relpath(p, os.path.dirname(path))
                include_file(p, basename)
        else:
            # Copy file
            basename = os.path.basename(path)
            include_file(path, basename)


@coroutine_with_priority(-1000.0)
async def _add_platformio_options(pio_options):
    # Add includes at the very end, so that they override everything
    for key, val in pio_options.items():
        if key == "build_flags" and not isinstance(val, list):
            val = [val]
        cg.add_platformio_option(key, val)


@coroutine_with_priority(30.0)
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


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(cg.global_ns.namespace("esphome").using)
    # These can be used by user lambdas, put them to default scope
    cg.add_global(cg.RawExpression("using std::isnan"))
    cg.add_global(cg.RawExpression("using std::min"))
    cg.add_global(cg.RawExpression("using std::max"))

    cg.add(
        cg.App.pre_setup(
            config[CONF_NAME],
            config[CONF_FRIENDLY_NAME],
            config[CONF_AREA],
            config.get(CONF_COMMENT, ""),
            cg.RawExpression('__DATE__ ", " __TIME__'),
            config[CONF_NAME_ADD_MAC_SUFFIX],
        )
    )

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

    if CORE.using_arduino and not CORE.is_bk72xx:
        CORE.add_job(add_arduino_global_workaround)

    if config[CONF_INCLUDES]:
        # Get the <...> includes
        system_includes = []
        other_includes = []
        for include in config[CONF_INCLUDES]:
            if include.startswith("<") and include.endswith(">"):
                system_includes.append(include)
            else:
                other_includes.append(include)
        # <...> includes should be at the start
        for include in system_includes:
            cg.add_global(cg.RawStatement(f"#include {include}"), prepend=True)
        # Other includes should be at the end
        CORE.add_job(add_includes, other_includes)

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
