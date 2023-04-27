import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NAME,
    CONF_PROJECT,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    __version__,
)
from esphome.core import CORE

from .const import KEY_BOARD, KEY_LIBRETINY, LTComponent

# force import gpio to register pin schema
from .gpio import libretiny_pin_to_code  # noqa


_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@kuba2k2"]
AUTO_LOAD = []


def _set_core_data(config):
    CORE.data[KEY_LIBRETINY] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "libretiny"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_LIBRETINY][KEY_BOARD] = config[CONF_BOARD]
    return config


# NOTE: Keep this in mind when updating the recommended version:
#  * For all constants below, update platformio.ini (in this repo)
ARDUINO_VERSIONS = {
    "dev": (cv.Version(0, 0, 0), "https://github.com/kuba2k2/libretiny.git"),
    "latest": (cv.Version(0, 0, 0), None),
    "recommended": (cv.Version(1, 0, 0), None),
}


def _check_framework_version(value):
    value = value.copy()

    if value[CONF_VERSION] in ARDUINO_VERSIONS:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = ARDUINO_VERSIONS[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source

    return value


CONF_LT_CONFIG = "lt_config"
CONF_LOGLEVEL = "loglevel"
CONF_SDK_SILENT = "sdk_silent"
CONF_SDK_SILENT_ALL = "sdk_silent_all"
CONF_GPIO_RECOVER = "gpio_recover"

LT_LOGLEVELS = [
    "VERBOSE",
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL",
]

FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
        }
    ),
    _check_framework_version,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LTComponent),
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
            cv.Optional(CONF_LT_CONFIG, default={}): {
                cv.string_strict: cv.string,
            },
            cv.Optional(CONF_LOGLEVEL, default="warn"): cv.one_of(
                *LT_LOGLEVELS, upper=True
            ),
            cv.Optional(CONF_SDK_SILENT, default=True): cv.boolean,
            cv.Optional(CONF_SDK_SILENT_ALL, default=True): cv.boolean,
            cv.Optional(CONF_GPIO_RECOVER, default=True): cv.boolean,
        },
    ),
    _set_core_data,
)


# pylint: disable=use-dict-literal
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    # setup board config
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_LIBRETINY")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "LibreTiny")

    # setup LT logger to work nicely with ESPHome logger
    lt_config = dict(
        LT_LOGLEVEL="LT_LEVEL_" + config[CONF_LOGLEVEL],
        LT_LOGGER_CALLER=0,
        LT_LOGGER_TASK=0,
        LT_LOGGER_COLOR=1,
        LT_DEBUG_ALL=1,
        LT_UART_SILENT_ENABLED=int(config[CONF_SDK_SILENT]),
        LT_UART_SILENT_ALL=int(config[CONF_SDK_SILENT_ALL]),
        LT_USE_TIME=1,
    )
    lt_config.update(config[CONF_LT_CONFIG])

    # force using arduino framework
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")

    # disable library compatibility checks
    cg.add_platformio_option("lib_ldf_mode", "off")
    # include <Arduino.h> in every file
    cg.add_platformio_option("build_src_flags", "-include Arduino.h")

    # if platform version is a valid version constraint, prefix the default package
    framework = config[CONF_FRAMEWORK]
    cv.platformio_version_constraint(framework[CONF_VERSION])
    if str(framework[CONF_VERSION]) != "0.0.0":
        cg.add_platformio_option("platform", f"libretiny @ {framework[CONF_VERSION]}")
    elif framework[CONF_SOURCE]:
        cg.add_platformio_option("platform", framework[CONF_SOURCE])
    else:
        cg.add_platformio_option("platform", "libretiny")

    # add LT configuration options
    for name, value in sorted(lt_config.items()):
        cg.add_build_flag(f"-D{name}={value}")

    # add ESPHome LT-related options
    cg.add_define("LT_GPIO_RECOVER", int(config[CONF_GPIO_RECOVER]))

    # dummy version code
    cg.add_define("USE_ARDUINO_VERSION_CODE", cg.RawExpression("VERSION_CODE(0, 0, 0)"))

    # decrease web server stack size (16k words -> 4k words)
    cg.add_build_flag("-DCONFIG_ASYNC_TCP_STACK_SIZE=4096")

    # custom output firmware name and version
    if CONF_PROJECT in config:
        cg.add_platformio_option(
            "custom_fw_name", "esphome." + config[CONF_PROJECT][CONF_NAME]
        )
        cg.add_platformio_option(
            "custom_fw_version", config[CONF_PROJECT][CONF_VERSION]
        )
    else:
        cg.add_platformio_option("custom_fw_name", "esphome")
        cg.add_platformio_option("custom_fw_version", __version__)

    await cg.register_component(var, config)
