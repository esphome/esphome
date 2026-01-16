from __future__ import annotations

import asyncio
import logging
from pathlib import Path

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr import (
    Section,
    copy_files as zephyr_copy_files,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_set_core_data,
    zephyr_setup_preferences,
    zephyr_to_code,
)
from esphome.components.zephyr.const import KEY_BOARD, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_NAME,
    CONF_RESET_PIN,
    CONF_SIZE,
    CONF_TYPE,
    CONF_VERSION,
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

from .boards import BOARDS, BOOTLOADER_PARTITION_MAP, FLASH_SIZE_MAP, Nrf52Board
from .const import (
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_NONE,
    BOOTLOADER_NORDIC,
    CONF_BOOTLOADER,
    CONF_EXT_FLASH,
    CONF_FLASH_SIZE,
    CONF_LABEL,
    CONF_MCU,
    CONF_PARTITIONS,
    MCU_FAMILY_NRF52,
)

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["zephyr", "preferences"]
IS_TARGET_PLATFORM = True
_LOGGER = logging.getLogger(__name__)


def _set_platform(config: ConfigType) -> ConfigType:
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    return config


def _get_board_info(config):
    board_config = config[CONF_BOARD]
    board_id = board_config[CONF_NAME]
    if "/" in board_id:
        board_id = board_id.split("/")[0]
    board_info = BOARDS[board_id].copy()
    board_info[CONF_ID] = board_id
    board_info[CONF_NAME] = board_config[CONF_NAME]

    if CONF_FLASH_SIZE in board_config:
        board_info[CONF_FLASH_SIZE] = board_config[CONF_FLASH_SIZE]
    else:
        board_info[CONF_FLASH_SIZE] = FLASH_SIZE_MAP[board_info[CONF_MCU]]

    if CONF_EXT_FLASH in board_config:
        board_info[CONF_EXT_FLASH] = board_config[CONF_EXT_FLASH]

    if CONF_BOOTLOADER in config:
        bootloader = config[CONF_BOOTLOADER]
        if bootloader == BOOTLOADER_ADAFRUIT_NRF52_SD132:
            # From our perspective SD132 and SD140v6 is the same
            bootloader = BOOTLOADER_ADAFRUIT_NRF52_SD140_V6
        if bootloader in (
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
            BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
            BOOTLOADER_NORDIC,
        ):
            board_info[CONF_BOOTLOADER] = {
                CONF_TYPE: bootloader,
                CONF_PARTITIONS: BOOTLOADER_PARTITION_MAP[bootloader],
            }
        else:
            board_info[CONF_BOOTLOADER] = None

    return board_info


def _set_core_data(config):
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR
    version = cv.Version.parse(cv.version_number(config[CONF_FRAMEWORK][CONF_VERSION]))
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = version

    info = _get_board_info(config)
    board = Nrf52Board.from_dict(info)
    zephyr_set_core_data(config, board)

    if board.bootloader:
        sections = [
            Section(p["name"], p["address"], p["size"], "flash_primary")
            for p in board.bootloader[CONF_PARTITIONS]
        ]
        zephyr_add_pm_static(sections)

    return config


BOOTLOADERS = [
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_NONE,
    BOOTLOADER_NORDIC,
]

nrf52_ns = cg.esphome_ns.namespace("nrf52")
DeviceFirmwareUpdate = nrf52_ns.class_("DeviceFirmwareUpdate", cg.Component)

CONF_DFU = "dfu"
CONF_DCDC = "dcdc"
CONF_REG0 = "reg0"
CONF_UICR_ERASE = "uicr_erase"

VOLTAGE_LEVELS = [1.8, 2.1, 2.4, 2.7, 3.0, 3.3]


def _parse_shorthand_board(value: str) -> ConfigType:
    return {CONF_NAME: value}


BOARD_SCHEMA = cv.All(
    cv.Any(
        cv.All(cv.string_strict, _parse_shorthand_board),
        cv.Schema(
            {
                cv.Required(CONF_NAME): cv.string_strict,
                cv.Optional(CONF_EXT_FLASH): cv.Schema(
                    {
                        cv.Required(CONF_LABEL): cv.string_strict,
                        cv.Required(CONF_SIZE): cv.hex_int,
                    }
                ),
                cv.Optional(CONF_FLASH_SIZE): cv.hex_int,
            }
        ),
    ),
)

CONFIG_SCHEMA = cv.All(
    _set_platform,
    cv.Schema(
        {
            cv.Required(CONF_BOARD): BOARD_SCHEMA,
            cv.Optional(CONF_BOOTLOADER): cv.one_of(*BOOTLOADERS, lower=True),
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
            cv.Optional(CONF_FRAMEWORK, default={CONF_VERSION: "2.6.1-7"}): cv.Schema(
                {
                    cv.Required(CONF_VERSION): cv.string_strict,
                }
            ),
        }
    ),
    _set_core_data,
)


def _validate_dfu():
    bootloader = zephyr_data()[KEY_BOARD].bootloader
    if not bootloader:
        raise cv.Invalid("DFU requires a bootloader to be configured")
    if bootloader[CONF_TYPE] not in (
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
        BOOTLOADER_NORDIC,
    ):
        raise cv.Invalid(f"'{bootloader[CONF_TYPE]}' bootloader does not support DFU")


def _final_validate(config):
    if CONF_DFU in config:
        _validate_dfu()

    return config


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""
    board = zephyr_data()[KEY_BOARD]
    cg.add_platformio_option("board", board.id)
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", board.id)
    cg.add_define("ESPHOME_VARIANT", board.mcu)
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
            f"platformio/framework-zephyr@https://github.com/tomaszduda23/framework-sdk-nrf/archive/refs/tags/v{CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]}.zip",
            "platformio/toolchain-gccarmnoneeabi@https://github.com/tomaszduda23/toolchain-sdk-ng/archive/refs/tags/v0.17.4-0.zip",
        ],
    )

    if board.bootloader and board.bootloader[CONF_TYPE] == BOOTLOADER_MCUBOOT:
        cg.add_define("USE_BOOTLOADER_MCUBOOT")

    if board.bootloader and board.bootloader[CONF_TYPE] in (
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    ):
        # make sure that firmware.zip is created
        # for Adafruit_nRF52_Bootloader
        cg.add_platformio_option("board_upload.protocol", "nrfutil")
        cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
        cg.add_platformio_option("board_upload.require_upload_port", "true")
        cg.add_platformio_option("board_upload.wait_for_upload_port", "true")

    zephyr_setup_preferences()
    zephyr_to_code(config)

    if dfu_config := config.get(CONF_DFU):
        CORE.add_job(_dfu_to_code, dfu_config)
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]

    if board.mcu in MCU_FAMILY_NRF52:
        if framework_ver < cv.Version(2, 9, 2):
            zephyr_add_prj_conf("BOARD_ENABLE_DCDC", config[CONF_DCDC])
        else:
            zephyr_add_overlay(
                f"""
                    &reg1 {{
                        regulator-initial-mode = <{"NRF5X_REG_MODE_DCDC" if config[CONF_DCDC] else "NRF5X_REG_MODE_LDO"}>;
                    }};
                """
            )

        if reg0_config := config.get(CONF_REG0):
            value = VOLTAGE_LEVELS.index(reg0_config[CONF_VOLTAGE])
            cg.add_define("USE_NRF52_REG0_VOUT", value)
            if reg0_config[CONF_UICR_ERASE]:
                cg.add_define("USE_NRF52_UICR_ERASE")

            # use NFC pins as GPIO
            if framework_ver < cv.Version(2, 9, 2):
                zephyr_add_prj_conf("NFCT_PINS_AS_GPIOS", True)
            else:
                zephyr_add_overlay(
                    """
                        &uicr {
                            nfct-pins-as-gpios;
                        };
                    """
                )

    # c++ support
    if framework_ver < cv.Version(2, 9, 2):
        zephyr_add_prj_conf("CPLUSPLUS", True)
        zephyr_add_prj_conf("LIB_CPLUSPLUS", True)
    else:
        zephyr_add_prj_conf("CPP", True)
        zephyr_add_prj_conf("REQUIRES_FULL_LIBCPP", True)
    # watchdog
    zephyr_add_prj_conf("WATCHDOG", True)
    zephyr_add_prj_conf("WDT_DISABLE_AT_BOOT", False)
    # disable console
    zephyr_add_prj_conf("UART_CONSOLE", False)
    zephyr_add_prj_conf("CONSOLE", False)


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
