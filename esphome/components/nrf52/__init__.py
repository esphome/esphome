import esphome.codegen as cg
from esphome.components.zephyr import zephyr_set_core_data, zephyr_to_code
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_ZEPHYR,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_PLATFORM_VERSION,
    CONF_TYPE,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, coroutine_with_priority

from .boards_zephyr import BOARDS_ZEPHYR
from .const import BOOTLOADER_ADAFRUIT

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["zephyr"]
PLATFORM_NRF52 = 'nrf52'

def set_core_data(config):
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    return config


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
    elif bootloader is not None and bootloader != value[KEY_BOOTLOADER]:
        raise cv.Invalid(
            f"{value[CONF_FRAMEWORK][CONF_TYPE]} does not support '{bootloader}' bootloader for {value[CONF_BOARD]}"
        )
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
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
    conf = {CONF_PLATFORM_VERSION: "platformio/nordicnrf52@10.3.0"}
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])
    cg.add_platformio_option(
        "platform_packages",
        [
            "platformio/framework-zephyr@https://github.com/tomaszduda23/framework-sdk-nrf/archive/refs/tags/v2.6.1-1.zip",
            "platformio/toolchain-gccarmnoneeabi@https://github.com/tomaszduda23/toolchain-sdk-ng/archive/refs/tags/v0.16.1.zip",
        ],
    )

    if config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT:
        # make sure that firmware.zip is created
        # for Adafruit_nRF52_Bootloader
        cg.add_platformio_option("board_upload.protocol", "nrfutil")
        cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
        cg.add_platformio_option("board_upload.require_upload_port", "true")
        cg.add_platformio_option("board_upload.wait_for_upload_port", "true")

    zephyr_to_code(conf)
