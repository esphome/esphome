import os
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
)
from esphome.core import CORE, coroutine_with_priority

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

AUTO_LOAD = ["nrf52_nrfx"]


def set_core_data(config):
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    return config


# https://github.com/platformio/platform-nordicnrf52/releases
ARDUINO_PLATFORM_VERSION = cv.Version(10, 3, 0)


def _arduino_check_versions(value):
    value = value.copy()
    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ARDUINO_PLATFORM_VERSION))
    )
    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/nordicnrf52@{value}"
    except cv.Invalid:
        return value


CONF_PLATFORM_VERSION = "platform_version"

ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
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

nrf52_ns = cg.esphome_ns.namespace("nrf52")


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add(nrf52_ns.setup_preferences())
    if config[CONF_BOARD] == "nrf52840":
        # it has most generic GPIO mapping
        config[CONF_BOARD] = "nrf52840_dk_adafruit"
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "nrf52840")
    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("framework", "arduino")
    cg.add_build_flag("-DUSE_ARDUINO")
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    # make sure that firmware.zip is created
    cg.add_platformio_option("board_upload.protocol", "nrfutil")
    cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
    cg.add_platformio_option("board_upload.require_upload_port", "true")
    cg.add_platformio_option("board_upload.wait_for_upload_port", "true")
    # watchdog
    cg.add_build_flag("-DNRFX_WDT_ENABLED=1")
    cg.add_build_flag("-DNRFX_WDT0_ENABLED=1")
    cg.add_build_flag("-DNRFX_WDT_CONFIG_NO_IRQ=1")
    # prevent setting up GPIO PINs
    cg.add_platformio_option("board_build.variant", "nrf52840")
    cg.add_platformio_option(
        "board_build.variants_dir", os.path.dirname(os.path.realpath(__file__))
    )
