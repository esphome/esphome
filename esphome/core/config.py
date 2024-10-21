import logging
import multiprocessing
import os
import re

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ARDUINO_VERSION,
    CONF_AREA,
    CONF_BOARD,
    CONF_BOARD_FLASH_MODE,
    CONF_BUILD_PATH,
    CONF_COMMENT,
    CONF_COMPILE_PROCESS_LIMIT,
    CONF_ESPHOME,
    CONF_FRAMEWORK,
    CONF_FRIENDLY_NAME,
    CONF_INCLUDES,
    CONF_LIBRARIES,
    CONF_MIN_VERSION,
    CONF_NAME,
    CONF_ON_BOOT,
    CONF_ON_LOOP,
    CONF_ON_SHUTDOWN,
    CONF_ON_UPDATE,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PRIORITY,
    CONF_PROJECT,
    CONF_SOURCE,
    CONF_TRIGGER_ID,
    CONF_TYPE,
    CONF_VERSION,
    KEY_CORE,
    PLATFORM_ESP8266,
    TARGET_PLATFORMS,
    __version__ as ESPHOME_VERSION,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, get_str_env, walk_files

_LOGGER = logging.getLogger(__name__)

BUILD_FLASH_MODES = ["qio", "qout", "dio", "dout"]
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

VERSION_REGEX = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:[ab]\d+)?$")

CONF_NAME_ADD_MAC_SUFFIX = "name_add_mac_suffix"


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


CONF_ESP8266_RESTORE_FROM_FLASH = "esp8266_restore_from_flash"
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
        # Compat options, these were moved to target-platform specific sections
        # but we'll keep these around for a long time because every config would
        # be impacted
        cv.Optional(CONF_PLATFORM): cv.one_of(*TARGET_PLATFORMS, lower=True),
        cv.Optional(CONF_BOARD): cv.string_strict,
        cv.Optional(CONF_ESP8266_RESTORE_FROM_FLASH): cv.valid,
        cv.Optional(CONF_BOARD_FLASH_MODE): cv.valid,
        cv.Optional(CONF_ARDUINO_VERSION): cv.valid,
    },
    extra=cv.ALLOW_EXTRA,
)


def preload_core_config(config, result):
    with cv.prepend_path(CONF_ESPHOME):
        conf = PRELOAD_CONFIG_SCHEMA(config[CONF_ESPHOME])

    CORE.name = conf[CONF_NAME]
    CORE.friendly_name = conf.get(CONF_FRIENDLY_NAME)
    CORE.data[KEY_CORE] = {}

    if CONF_BUILD_PATH not in conf:
        build_path = get_str_env("ESPHOME_BUILD_PATH", "build")
        conf[CONF_BUILD_PATH] = os.path.join(build_path, CORE.name)
    CORE.build_path = CORE.relative_internal_path(conf[CONF_BUILD_PATH])

    has_oldstyle = CONF_PLATFORM in conf
    newstyle_found = [key for key in TARGET_PLATFORMS if key in config]
    oldstyle_opts = [
        CONF_ESP8266_RESTORE_FROM_FLASH,
        CONF_BOARD_FLASH_MODE,
        CONF_ARDUINO_VERSION,
        CONF_BOARD,
    ]

    if not has_oldstyle and not newstyle_found:
        raise cv.Invalid(
            "Platform missing. You must include one of the available platform keys: "
            + ", ".join(TARGET_PLATFORMS),
            [CONF_ESPHOME],
        )
    if has_oldstyle and newstyle_found:
        raise cv.Invalid(
            f"Please remove the `platform` key from the [esphome] block. You're already using the new style with the [{conf[CONF_PLATFORM]}] block",
            [CONF_ESPHOME, CONF_PLATFORM],
        )
    if len(newstyle_found) > 1:
        raise cv.Invalid(
            f"Found multiple target platform blocks: {', '.join(newstyle_found)}. Only one is allowed.",
            [newstyle_found[0]],
        )
    if newstyle_found:
        # Convert to newstyle
        for key in oldstyle_opts:
            if key in conf:
                raise cv.Invalid(
                    f"Please move {key} to the [{newstyle_found[0]}] block.",
                    [CONF_ESPHOME, key],
                )

    if has_oldstyle:
        plat = conf.pop(CONF_PLATFORM)
        plat_conf = {}
        if CONF_ESP8266_RESTORE_FROM_FLASH in conf:
            plat_conf["restore_from_flash"] = conf.pop(CONF_ESP8266_RESTORE_FROM_FLASH)
        if CONF_BOARD_FLASH_MODE in conf:
            plat_conf[CONF_BOARD_FLASH_MODE] = conf.pop(CONF_BOARD_FLASH_MODE)
        if CONF_ARDUINO_VERSION in conf:
            plat_conf[CONF_FRAMEWORK] = {}
            if plat != PLATFORM_ESP8266:
                plat_conf[CONF_FRAMEWORK][CONF_TYPE] = "arduino"

            try:
                if conf[CONF_ARDUINO_VERSION] not in ("recommended", "latest", "dev"):
                    cv.Version.parse(conf[CONF_ARDUINO_VERSION])
                plat_conf[CONF_FRAMEWORK][CONF_VERSION] = conf.pop(CONF_ARDUINO_VERSION)
            except ValueError:
                plat_conf[CONF_FRAMEWORK][CONF_SOURCE] = conf.pop(CONF_ARDUINO_VERSION)
        if CONF_BOARD in conf:
            plat_conf[CONF_BOARD] = conf.pop(CONF_BOARD)
        # Insert generated target platform config to main config
        config[plat] = plat_conf
    config[CONF_ESPHOME] = conf


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
        CORE.add_job(add_includes, config[CONF_INCLUDES])

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
