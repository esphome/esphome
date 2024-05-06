import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
    CONF_TYPE,
    CONF_FRAMEWORK,
    CONF_VARIANT,
    CONF_PLATFORM_VERSION,
)
from esphome.core import CORE, coroutine_with_priority

from esphome.components.zephyr import (
    zephyr_set_core_data,
    zephyr_to_code,
)
from esphome.components.zephyr.const import (
    ZEPHYR_VARIANT_GENERIC,
    ZEPHYR_VARIANT_NRF_SDK,
    KEY_ZEPHYR,
    KEY_BOOTLOADER,
    BOOTLOADER_MCUBOOT,
)
from .boards_zephyr import BOARDS_ZEPHYR
from .const import (
    BOOTLOADER_ADAFRUIT,
)

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

AUTO_LOAD = ["zephyr"]


def set_core_data(config):
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    return config


# https://github.com/platformio/platform-nordicnrf52/releases
NORDICNRF52_PLATFORM_VERSION = cv.Version(10, 3, 0)


def _platform_check_versions(value):
    value = value.copy()
    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION,
        _parse_platform_version(str(NORDICNRF52_PLATFORM_VERSION)),
    )
    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/nordicnrf52@{value}"
    except cv.Invalid:
        return value


PLATFORM_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _platform_check_versions,
)

ZEPHYR_VARIANTS = [
    ZEPHYR_VARIANT_GENERIC,
    ZEPHYR_VARIANT_NRF_SDK,
]

FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VARIANT, default=ZEPHYR_VARIANT_NRF_SDK): cv.one_of(
                *ZEPHYR_VARIANTS, lower=True
            ),
        }
    ),
    _platform_check_versions,
)

BOOTLOADERS = [
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_MCUBOOT,
]


def _detect_bootloader(value):
    value = value.copy()
    bootloader = None

    if (
        value[CONF_BOARD] in BOARDS_ZEPHYR
        and KEY_BOOTLOADER in BOARDS_ZEPHYR[value[CONF_BOARD]]
    ):
        bootloader = BOARDS_ZEPHYR[value[CONF_BOARD]][KEY_BOOTLOADER]

    if KEY_BOOTLOADER not in value:
        if bootloader is None:
            bootloader = BOOTLOADER_MCUBOOT
        value[KEY_BOOTLOADER] = bootloader
    else:
        if bootloader is not None and bootloader != value[KEY_BOOTLOADER]:
            raise cv.Invalid(
                f"{value[CONF_FRAMEWORK][CONF_TYPE]} does not support '{bootloader}' bootloader for {value[CONF_BOARD]}"
            )
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
            cv.Optional(KEY_BOOTLOADER): cv.one_of(*BOOTLOADERS, lower=True),
        }
    ),
    _detect_bootloader,
    set_core_data,
)


@coroutine_with_priority(1000)
async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])

    if config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT:
        # make sure that firmware.zip is created
        # for Adafruit_nRF52_Bootloader
        cg.add_platformio_option("board_upload.protocol", "nrfutil")
        cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
        cg.add_platformio_option("board_upload.require_upload_port", "true")
        cg.add_platformio_option("board_upload.wait_for_upload_port", "true")
    #
    if CORE.using_zephyr:
        zephyr_to_code(conf)
    else:
        raise NotImplementedError
