import logging
import os
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, boards
from esphome.const import (
    CONF_ARDUINO_VERSION,
    CONF_BOARD,
    CONF_BOARD_FLASH_MODE,
    CONF_BUILD_PATH,
    CONF_COMMENT,
    CONF_ESPHOME,
    CONF_INCLUDES,
    CONF_LIBRARIES,
    CONF_MCU,
    CONF_NAME,
    CONF_ON_BOOT,
    CONF_ON_LOOP,
    CONF_ON_SHUTDOWN,
    CONF_PLATFORM,
    CONF_PLATFORMIO_OPTIONS,
    CONF_PRIORITY,
    CONF_PROJECT,
    CONF_TRIGGER_ID,
    CONF_ESP8266_RESTORE_FROM_FLASH,
    CONF_VERSION,
    ESP_PLATFORMS,
    ESP_MCUS,
    ESP_MCU_ESP8266,
    ESP_MCU_ESP32,
    ESP_MCU_ESP32C3,
    ESP_MCU_ESP32S2,
    ESP_MCU_ESP32S3,
    PLATFORMIO_ESP8266_LUT,
    PLATFORMIO_ESP32_LUT,
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


def validate_mcu_board(schema):
    boardlist = {
        ESP_MCU_ESP8266: list(boards.ESP8266_BOARD_PINS.keys()),
        ESP_MCU_ESP32: list(boards.ESP32_BOARD_PINS.keys()),
        ESP_MCU_ESP32C3: list(boards.ESP32_C3_BOARD_PINS.keys()),
        ESP_MCU_ESP32S2: [],
        ESP_MCU_ESP32S3: [],
    }

    for mcu, mcu_boards in boardlist.items():
        if schema[CONF_BOARD] not in mcu_boards:
            continue

        # allow but warn if different MCU is set (some generic boards can be used for multiple MCUs)
        if CONF_MCU in schema and schema[CONF_MCU] != mcu:
            _LOGGER.warning(
                "Board '%s' is known with MCU %s instead of %s.",
                schema[CONF_BOARD],
                mcu,
                schema[CONF_MCU],
            )
        elif CONF_MCU not in schema:
            schema[CONF_MCU] = mcu
        break
    else:
        if CONF_MCU not in schema:
            raise cv.Invalid(
                f"Unknown board '{schema[CONF_BOARD]}' used, please specify the MCU.",
                [CONF_MCU],
            )

        _LOGGER.warning("Using unknown board '%s'.", schema[CONF_BOARD])

    if schema[CONF_MCU] not in ESP_MCUS[schema[CONF_PLATFORM]]:
        raise cv.Invalid(
            f"MCU {schema[CONF_MCU]} is not valid for platform {schema[CONF_PLATFORM]}.",
            [CONF_MCU],
        )

    return schema


def validate_arduino_version(value):
    value = cv.string_strict(value)
    value_ = value.upper()
    if CORE.is_esp8266:
        if (
            VERSION_REGEX.match(value) is not None
            and value_ not in PLATFORMIO_ESP8266_LUT
        ):
            raise cv.Invalid(
                "Unfortunately the arduino framework version '{}' is unsupported "
                "at this time. You can override this by manually using "
                "espressif8266@<platformio version>".format(value)
            )
        if value_ in PLATFORMIO_ESP8266_LUT:
            return PLATFORMIO_ESP8266_LUT[value_]
        return value
    if CORE.is_esp32:
        if (
            VERSION_REGEX.match(value) is not None
            and value_ not in PLATFORMIO_ESP32_LUT
        ):
            raise cv.Invalid(
                "Unfortunately the arduino framework version '{}' is unsupported "
                "at this time. You can override this by manually using "
                "espressif32@<platformio version>".format(value)
            )
        if value_ in PLATFORMIO_ESP32_LUT:
            return PLATFORMIO_ESP32_LUT[value_]
        return value
    raise NotImplementedError


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


PRELOAD_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.hostname,
        cv.Required(CONF_PLATFORM): cv.one_of(*ESP_PLATFORMS, upper=True),
        cv.Optional(CONF_MCU): cv.one_of(
            *(mcu for platform in ESP_MCUS.values() for mcu in platform)
        ),
        cv.Required(CONF_BOARD): cv.string,
        cv.Optional(CONF_BUILD_PATH): cv.string,
    },
    extra=cv.ALLOW_EXTRA,
)

# A cv.All() schema cannot be extended, so this needs to be defined separately. Also causes
# validate_mcu_board() to only run for the preload validation (so warnings are logged only once).
PRELOAD_CONFIG_VALIDATE = cv.All(PRELOAD_CONFIG_SCHEMA, validate_mcu_board)

CONFIG_SCHEMA = PRELOAD_CONFIG_SCHEMA.extend(
    {
        cv.Optional(CONF_COMMENT): cv.string,
        cv.Optional(
            CONF_ARDUINO_VERSION, default="recommended"
        ): validate_arduino_version,
        cv.Optional(CONF_PLATFORMIO_OPTIONS, default={}): cv.Schema(
            {
                cv.string_strict: cv.Any([cv.string], cv.string),
            }
        ),
        cv.SplitDefault(CONF_ESP8266_RESTORE_FROM_FLASH, esp8266=False): cv.All(
            cv.only_on_esp8266, cv.boolean
        ),
        cv.SplitDefault(CONF_BOARD_FLASH_MODE, esp8266="dout"): cv.one_of(
            *BUILD_FLASH_MODES, lower=True
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
    },
    extra=cv.PREVENT_EXTRA,
)


def preload_core_config(config):
    core_key = "esphome"
    if "esphomeyaml" in config:
        _LOGGER.warning(
            "The esphomeyaml section has been renamed to esphome in 1.11.0. "
            "Please replace 'esphomeyaml:' in your configuration with 'esphome:'."
        )
        config[CONF_ESPHOME] = config.pop("esphomeyaml")
        core_key = "esphomeyaml"
    if CONF_ESPHOME not in config:
        raise cv.RequiredFieldInvalid("required key not provided", CONF_ESPHOME)
    with cv.prepend_path(core_key):
        out = PRELOAD_CONFIG_VALIDATE(config[CONF_ESPHOME])
    CORE.name = out[CONF_NAME]
    CORE.esp_platform = out[CONF_PLATFORM]
    CORE.mcu = out[CONF_MCU]
    CORE.board = out[CONF_BOARD]
    CORE.build_path = CORE.relative_config_path(out.get(CONF_BUILD_PATH, CORE.name))


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

    cg.add_define("ESPHOME_PLATFORM_" + CORE.esp_platform, 1)
    cg.add_define("ESPHOME_MCU_" + CORE.mcu.replace("-", "_"), 1)
    cg.add_define("ESPHOME_BOARD", CORE.board)

    if CONF_PROJECT in config:
        cg.add_define("ESPHOME_PROJECT_NAME", config[CONF_PROJECT][CONF_NAME])
        cg.add_define("ESPHOME_PROJECT_VERSION", config[CONF_PROJECT][CONF_VERSION])
