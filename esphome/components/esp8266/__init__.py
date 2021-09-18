from contextlib import suppress

from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import KEY_BOARD, KEY_ESP8266

# force import gpio to register pin schema
from .gpio import esp8266_pin_to_code  # noqa


CODEOWNERS = ["@esphome/core"]


def set_core_data(config):
    CORE.data[KEY_ESP8266] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "esp8266"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION_HINT]
    )
    CORE.data[KEY_ESP8266][KEY_BOARD] = config[CONF_BOARD]
    return config


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/esp8266/Arduino/releases) version to
    # a PIO platformio/framework-arduinoespressif8266 value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif8266
    if ver <= cv.Version(2, 4, 1):
        return f"~1.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    if ver <= cv.Version(2, 6, 2):
        return f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/esp8266/Arduino/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif8266
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(2, 7, 4)
# The platformio/espressif8266 version to use for arduino 2 framework versions
#  - https://github.com/platformio/platform-espressif8266/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif8266
ARDUINO_2_PLATFORM_VERSION = cv.Version(2, 6, 2)
# for arduino 3 framework versions
ARDUINO_3_PLATFORM_VERSION = cv.Version(3, 0, 2)


def _arduino_check_versions(value):
    lookups = {
        "dev": ("https://github.com/esp8266/Arduino.git", cv.Version(3, 0, 2)),
        "latest": ("", cv.Version(3, 0, 2)),
        "recommended": (
            _format_framework_arduino_version(RECOMMENDED_ARDUINO_FRAMEWORK_VERSION),
            RECOMMENDED_ARDUINO_FRAMEWORK_VERSION,
        ),
    }
    ver_value = value[CONF_VERSION]
    default_ver_hint = None
    if ver_value.lower() in lookups:
        default_ver_hint = str(lookups[ver_value.lower()][1])
        ver_value = lookups[ver_value.lower()][0]
    else:
        with suppress(cv.Invalid):
            ver = cv.Version.parse(cv.version_number(value))
            if ver <= cv.Version(2, 4, 1):
                ver_value = f"~1.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            elif ver <= cv.Version(2, 6, 2):
                ver_value = f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            else:
                ver_value = f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            default_ver_hint = str(ver)

    if CONF_VERSION_HINT not in value and default_ver_hint is None:
        raise cv.Invalid("Needs a version hint to understand the framework version")

    ver_hint_s = value.get(CONF_VERSION_HINT, default_ver_hint)
    plat_ver = value.get(CONF_PLATFORM_VERSION)

    if plat_ver is None:
        ver_hint = cv.Version.parse(ver_hint_s)
        if ver_hint >= cv.Version(3, 0, 0):
            plat_ver = ARDUINO_3_PLATFORM_VERSION
        elif ver_hint >= cv.Version(2, 5, 0):
            plat_ver = ARDUINO_2_PLATFORM_VERSION
        else:
            plat_ver = cv.Version(1, 8, 0)

    return {
        CONF_VERSION: ver_value,
        CONF_VERSION_HINT: ver_hint_s,
        CONF_PLATFORM_VERSION: str(plat_ver),
    }


CONF_VERSION_HINT = "version_hint"
CONF_PLATFORM_VERSION = "platform_version"
ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_VERSION_HINT): cv.version_number,
            cv.Optional(CONF_PLATFORM_VERSION): cv.string_strict,
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


async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_ESP8266")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_build_flag("-DUSE_ESP8266_FRAMEWORK_ARDUINO")
    cg.add_platformio_option(
        "platform_packages",
        [f"platformio/framework-arduinoespressif8266 @ {conf[CONF_VERSION]}"],
    )
    cg.add_platformio_option(
        "platform", f"platformio/espressif8266 @ {conf[CONF_PLATFORM_VERSION]}"
    )

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
