import logging

from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from .const import KEY_RP2040, KEY_BOARD

from esphome.core import CORE, coroutine_with_priority
import esphome.config_validation as cv
import esphome.codegen as cg

# force import gpio to register pin schema
from .gpio import rp2040_pin_to_code  # noqa

_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@jesserockz"]
AUTO_LOAD = []


def set_core_data(config):
    CORE.data[KEY_RP2040] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "rp2040"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_RP2040][KEY_BOARD] = config[CONF_BOARD]

    return config


# NOTE: Keep this in mind when updating the recommended version:
#  * The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/arduino/ArduinoCore-mbed/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduino-mbed
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(2, 7, 2)

# The platformio/raspberrypi version to use for arduino frameworks
#  - https://github.com/platformio/platform-raspberrypi/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/raspberrypi
ARDUINO_PLATFORM_VERSION = cv.Version(1, 5, 0)


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(2, 7, 2), "https://github.com/arduino/ArduinoCore-mbed"),
        "latest": (cv.Version(2, 7, 2), None),
        "recommended": (RECOMMENDED_ARDUINO_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or f"~{version}"

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ARDUINO_PLATFORM_VERSION))
    )

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one."
        )

    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/raspberrypi @ {value}"
    except cv.Invalid:
        return value


CONF_PLATFORM_VERSION = "platform_version"

ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _arduino_check_versions,
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_FRAMEWORK, default={}): ARDUINO_FRAMEWORK_SCHEMA,
        }
    ),
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add_platformio_option("lib_ldf_mode", "off")

    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_RP2040")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2040")

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_RP2040_FRAMEWORK_ARDUINO")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [f"platformio/framework-arduino-mbed @ {conf[CONF_SOURCE]}"],
    )

    ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    cg.add_define(
        "USE_ARDUINO_VERSION_CODE",
        cg.RawExpression(f"VERSION_CODE({ver.major}, {ver.minor}, {ver.patch})"),
    )
