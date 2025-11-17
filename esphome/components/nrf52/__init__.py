from __future__ import annotations

import asyncio
import logging
from pathlib import Path

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr import (
    copy_files as zephyr_copy_files,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_set_core_data,
    zephyr_to_code,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOOTLOADER,
    KEY_ZEPHYR,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_RESET_PIN,
    CONF_VOLTAGE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
    ThreadModel,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.storage_json import StorageJSON
from esphome.types import ConfigType

from .boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG
from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
)

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["zephyr"]
IS_TARGET_PLATFORM = True
_LOGGER = logging.getLogger(__name__)


def set_core_data(config: ConfigType) -> ConfigType:
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version(2, 6, 1)

    if config[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        zephyr_add_pm_static(BOOTLOADER_CONFIG[config[KEY_BOOTLOADER]])

    return config


BOOTLOADERS = [
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_MCUBOOT,
]


def _detect_bootloader(config: ConfigType) -> ConfigType:
    """Detect the bootloader for the given board."""
    config = config.copy()
    bootloaders: list[str] = []
    board = config[CONF_BOARD]

    if board in BOARDS_ZEPHYR and KEY_BOOTLOADER in BOARDS_ZEPHYR[board]:
        # this board have bootloaders config available
        bootloaders = BOARDS_ZEPHYR[board][KEY_BOOTLOADER]

    if KEY_BOOTLOADER not in config:
        if bootloaders:
            # there is no bootloader in config -> take first one
            config[KEY_BOOTLOADER] = bootloaders[0]
        else:
            # make mcuboot as default if there is no configuration for that board
            config[KEY_BOOTLOADER] = BOOTLOADER_MCUBOOT
    elif bootloaders and config[KEY_BOOTLOADER] not in bootloaders:
        raise cv.Invalid(
            f"{board} does not support {config[KEY_BOOTLOADER]}, select one of: {', '.join(bootloaders)}"
        )
    return config


nrf52_ns = cg.esphome_ns.namespace("nrf52")
DeviceFirmwareUpdate = nrf52_ns.class_("DeviceFirmwareUpdate", cg.Component)

CONF_DFU = "dfu"
CONF_DCDC = "dcdc"
CONF_REG0 = "reg0"
CONF_UICR_ERASE = "uicr_erase"

VOLTAGE_LEVELS = [1.8, 2.1, 2.4, 2.7, 3.0, 3.3]

CONFIG_SCHEMA = cv.All(
    _detect_bootloader,
    set_core_data,
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(KEY_BOOTLOADER): cv.one_of(*BOOTLOADERS, lower=True),
            cv.Optional(CONF_DFU): cv.Schema(
                {
                    cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
                    cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
                }
            ),
            cv.Optional(CONF_DCDC, default=True): cv.boolean,
            cv.Optional(CONF_REG0): cv.Schema(
                {
                    cv.Required(CONF_VOLTAGE): cv.All(
                        cv.voltage,
                        cv.one_of(*VOLTAGE_LEVELS, float=True),
                    ),
                    cv.Optional(CONF_UICR_ERASE, default=False): cv.boolean,
                }
            ),
        }
    ),
)


def _validate_mcumgr(config):
    bootloader = zephyr_data()[KEY_BOOTLOADER]
    if bootloader == BOOTLOADER_MCUBOOT:
        raise cv.Invalid(f"'{bootloader}' bootloader does not support DFU")


def _final_validate(config):
    if CONF_DFU in config:
        _validate_mcumgr(config)
    if config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT:
        _LOGGER.warning(
            "Selected generic Adafruit bootloader. The board might crash. Consider settings `bootloader:`"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    # nRF52 processors are single-core
    cg.add_define(ThreadModel.SINGLE)
    cg.add_platformio_option(CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK])
    cg.add_platformio_option(
        "platform",
        "https://github.com/tomaszduda23/platform-nordicnrf52/archive/refs/tags/v10.3.0-1.zip",
    )
    cg.add_platformio_option(
        "platform_packages",
        [
            "platformio/framework-zephyr@https://github.com/tomaszduda23/framework-sdk-nrf/archive/refs/tags/v2.6.1-7.zip",
            "platformio/toolchain-gccarmnoneeabi@https://github.com/tomaszduda23/toolchain-sdk-ng/archive/refs/tags/v0.17.4-0.zip",
        ],
    )

    if config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        cg.add_define("USE_BOOTLOADER_MCUBOOT")
    else:
        if "_sd" in config[KEY_BOOTLOADER]:
            bootloader = config[KEY_BOOTLOADER].split("_")
            sd_id = bootloader[2][2:]
            cg.add_define("USE_SOFTDEVICE_ID", int(sd_id))
            if (len(bootloader)) > 3:
                sd_version = bootloader[3][1:]
                cg.add_define("USE_SOFTDEVICE_VERSION", int(sd_version))
        # make sure that firmware.zip is created
        # for Adafruit_nRF52_Bootloader
        cg.add_platformio_option("board_upload.protocol", "nrfutil")
        cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
        cg.add_platformio_option("board_upload.require_upload_port", "true")
        cg.add_platformio_option("board_upload.wait_for_upload_port", "true")

    zephyr_to_code(config)

    if dfu_config := config.get(CONF_DFU):
        CORE.add_job(_dfu_to_code, dfu_config)
    zephyr_add_prj_conf("BOARD_ENABLE_DCDC", config[CONF_DCDC])

    if reg0_config := config.get(CONF_REG0):
        value = VOLTAGE_LEVELS.index(reg0_config[CONF_VOLTAGE])
        cg.add_define("USE_NRF52_REG0_VOUT", value)
        if reg0_config[CONF_UICR_ERASE]:
            cg.add_define("USE_NRF52_UICR_ERASE")


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _dfu_to_code(dfu_config):
    cg.add_define("USE_NRF52_DFU")
    var = cg.new_Pvariable(dfu_config[CONF_ID])
    pin = await cg.gpio_pin_expression(dfu_config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(pin))
    zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
    await cg.register_component(var, dfu_config)


def copy_files() -> None:
    """Copy files to the build directory."""
    zephyr_copy_files()


def get_download_types(storage_json: StorageJSON) -> list[dict[str, str]]:
    """Get the download types for the firmware."""
    types = []
    UF2_PATH = "zephyr/zephyr.uf2"
    DFU_PATH = "firmware.zip"
    HEX_PATH = "zephyr/zephyr.hex"
    HEX_MERGED_PATH = "zephyr/merged.hex"
    APP_IMAGE_PATH = "zephyr/app_update.bin"
    build_dir = Path(storage_json.firmware_bin_path).parent
    if (build_dir / UF2_PATH).is_file():
        types = [
            {
                "title": "UF2 package (recommended)",
                "description": "For flashing via Adafruit nRF52 Bootloader as a flash drive.",
                "file": UF2_PATH,
                "download": f"{storage_json.name}.uf2",
            },
            {
                "title": "DFU package",
                "description": "For flashing via adafruit-nrfutil using USB CDC.",
                "file": DFU_PATH,
                "download": f"dfu-{storage_json.name}.zip",
            },
        ]
    else:
        types = [
            {
                "title": "HEX package",
                "description": "For flashing via pyocd using SWD.",
                "file": (
                    HEX_MERGED_PATH
                    if (build_dir / HEX_MERGED_PATH).is_file()
                    else HEX_PATH
                ),
                "download": f"{storage_json.name}.hex",
            },
        ]
        if (build_dir / APP_IMAGE_PATH).is_file():
            types += [
                {
                    "title": "App update package",
                    "description": "For flashing via mcumgr-web using BLE or smpclient using USB CDC.",
                    "file": APP_IMAGE_PATH,
                    "download": f"app-{storage_json.name}.img",
                },
            ]

    return types


def _upload_using_platformio(
    config: ConfigType, port: str, upload_args: list[str]
) -> int | str:
    from esphome import platformio_api

    if port is not None:
        upload_args += ["--upload-port", port]
    return platformio_api.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def upload_program(config: ConfigType, args, host: str) -> bool:
    from esphome.__main__ import check_permissions, get_port_type

    result = 0
    handled = False

    if get_port_type(host) == "SERIAL":
        check_permissions(host)
        result = _upload_using_platformio(config, host, ["-t", "upload"])
        handled = True

    if host == "PYOCD":
        result = _upload_using_platformio(config, host, ["-t", "flash_pyocd"])
        handled = True

    if result != 0:
        raise EsphomeError(f"Upload failed with result: {result}")

    return handled


def show_logs(config: ConfigType, args, devices: list[str]) -> bool:
    address = devices[0]
    from .ble_logger import is_mac_address, logger_connect, logger_scan

    if devices[0] == "BLE":
        ble_device = asyncio.run(logger_scan(CORE.config["esphome"]["name"]))
        if ble_device:
            address = ble_device.address
        else:
            return True

    if is_mac_address(address):
        asyncio.run(logger_connect(address))
        return True
    return False
