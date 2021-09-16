import logging
import os
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ARDUINO_VERSION,
    CONF_BOARD,
    CONF_BOARD_FLASH_MODE,
    CONF_BUILD_PATH,
    CONF_COMMENT,
    CONF_ESPHOME,
    CONF_INCLUDES,
    CONF_LIBRARIES,
    CONF_NAME,
    CONF_ON_BOOT,
    CONF_ON_LOOP,
    CONF_ON_SHUTDOWN,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PRIORITY,
    CONF_PROJECT,
    CONF_TRIGGER_ID,
    CONF_VERSION,
    KEY_CORE,
    TARGET_PLATFORMS,
)
from esphome.core import CORE, coroutine_with_priority
from esphome.helpers import copy_file_if_changed, walk_files

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

VERSION_REGEX = re.compile(r"^[0-9]+\.[0-9]+\.[0-9]+(?:[ab]\d+)?$")

CONF_NAME_ADD_MAC_SUFFIX = "name_add_mac_suffix"


VALID_INCLUDE_EXTS = {".h", ".hpp", ".tcc", ".ino", ".cpp", ".c"}


def valid_include(value):
    try:
        return cv.directory(value)
    except cv.Invalid:
        pass
    value = cv.file_(value)
    _, ext = os.path.splitext(value)
    if ext not in VALID_INCLUDE_EXTS:
        raise cv.Invalid(
            "Include has invalid file extension {} - valid extensions are {}"
            "".format(ext, ", ".join(VALID_INCLUDE_EXTS))
        )
    return value


def valid_project_name(value: str):
    if value.count(".") != 1:
        raise cv.Invalid("project name needs to have a namespace")

    value = value.replace(" ", "_")

    return value


CONF_ESP8266_RESTORE_FROM_FLASH = "esp8266_restore_from_flash"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.hostname,
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
                cv.Required(CONF_NAME): cv.All(cv.string_strict, valid_project_name),
                cv.Required(CONF_VERSION): cv.string_strict,
            }
        ),
    }
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
    CORE.data[KEY_CORE] = {}

    if CONF_BUILD_PATH not in conf:
        conf[CONF_BUILD_PATH] = CORE.name
    CORE.build_path = CORE.relative_config_path(conf[CONF_BUILD_PATH])

    has_oldstyle = CONF_PLATFORM in conf
    newstyle_found = [key for key in TARGET_PLATFORMS if key in config]
    oldstyle_opts = [
        CONF_ESP8266_RESTORE_FROM_FLASH,
        CONF_BOARD_FLASH_MODE,
        CONF_ARDUINO_VERSION,
        CONF_BOARD,
    ]

    if not has_oldstyle and not newstyle_found:
        raise cv.Invalid("Platform missing for core options!", [CONF_ESPHOME])
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
            plat_conf[CONF_ARDUINO_VERSION] = conf.pop(CONF_ARDUINO_VERSION)
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
        cg.add_platformio_option(key, val)


@coroutine_with_priority(-1000.0)
async def _esp8266_add_lwip_type():
    # If any component has already set this, do not change it
    if any(
        flag.startswith("-DPIO_FRAMEWORK_ARDUINO_LWIP2_") for flag in CORE.build_flags
    ):
        return

    # Default for platformio is LWIP2_LOW_MEMORY with:
    #  - MSS=536
    #  - LWIP_FEATURES enabled
    #     - this only adds some optional features like IP incoming packet reassembly and NAPT
    #       see also:
    #  https://github.com/esp8266/Arduino/blob/master/tools/sdk/lwip2/include/lwipopts.h

    # Instead we use LWIP2_HIGHER_BANDWIDTH_LOW_FLASH with:
    #  - MSS=1460
    #  - LWIP_FEATURES disabled (because we don't need them)
    # Other projects like Tasmota & ESPEasy also use this
    cg.add_build_flag("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")


@coroutine_with_priority(30.0)
async def _add_automations(config):
    for conf in config.get(CONF_ON_BOOT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], conf.get(CONF_PRIORITY))
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_SHUTDOWN, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_LOOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
        await cg.register_component(trigger, conf)
        await automation.build_automation(trigger, [], conf)


@coroutine_with_priority(100.0)
async def to_code(config):
    cg.add_global(cg.global_ns.namespace("esphome").using)
    cg.add(
        cg.App.pre_setup(
            config[CONF_NAME],
            cg.RawExpression('__DATE__ ", " __TIME__'),
            config[CONF_NAME_ADD_MAC_SUFFIX],
        )
    )

    CORE.add_job(_add_automations, config)

    # Set LWIP build constants for ESP8266
    if CORE.is_esp8266:
        CORE.add_job(_esp8266_add_lwip_type)

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

    if CORE.is_esp8266:
        # Arduino 2 has a non-standards conformant new that returns a nullptr instead of failing when
        # out of memory and exceptions are disabled. Since Arduino 2.6.0, this flag can be used to make
        # new abort instead. Use it so that OOM fails early (on allocation) instead of on dereference of
        # a NULL pointer (so the stacktrace makes more sense), and for consistency with Arduino 3,
        # which always aborts if exceptions are disabled.
        # For cases where nullptrs can be handled, use nothrow: `new (std::nothrow) T;`
        cg.add_build_flag("-DNEW_OOM_ABORT")

    cg.add_build_flag("-Wno-unused-variable")
    cg.add_build_flag("-Wno-unused-but-set-variable")
    cg.add_build_flag("-Wno-sign-compare")
    if config.get(CONF_ESP8266_RESTORE_FROM_FLASH, False):
        cg.add_define("USE_ESP8266_PREFERENCES_FLASH")

    if config[CONF_INCLUDES]:
        CORE.add_job(add_includes, config[CONF_INCLUDES])

    if CONF_PROJECT in config:
        cg.add_define("ESPHOME_PROJECT_NAME", config[CONF_PROJECT][CONF_NAME])
        cg.add_define("ESPHOME_PROJECT_VERSION", config[CONF_PROJECT][CONF_VERSION])

    if config[CONF_PLATFORMIO_OPTIONS]:
        CORE.add_job(_add_platformio_options, config[CONF_PLATFORMIO_OPTIONS])
