from __future__ import annotations

import asyncio
import logging
from pathlib import Path
import re
import subprocess

from esphome import pins
import esphome.codegen as cg
from esphome.components.zephyr import (
    HexValue,
    add_extra_script,
    copy_files as zephyr_copy_files,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_data,
    zephyr_set_core_data,
    zephyr_setup_preferences,
    zephyr_to_code,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    CONF_CDC_ACM,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_ZEPHYR,
    CdcAcm,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_DISABLED,
    CONF_ENABLE_OTA_ROLLBACK,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_OTA,
    CONF_RESET_PIN,
    CONF_SAFE_MODE,
    CONF_TOOLCHAIN,
    CONF_VERSION,
    CONF_VOLTAGE,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.core.config import BOARD_MAX_LENGTH
import esphome.final_validate as fv
from esphome.helpers import write_file_if_changed
from esphome.storage_json import StorageJSON
from esphome.types import ConfigType

from .boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG
from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
)
from .framework import check_and_install, get_component_cmakelists, build

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["zephyr", "preferences"]
IS_TARGET_PLATFORM = True
_LOGGER = logging.getLogger(__name__)

FAKE_BOARD_MANIFEST = """
{
    "frameworks": [
        "zephyr"
    ],
    "name": "esphome nrf52",
    "upload": {
        "maximum_ram_size": 248832,
        "maximum_size": 815104,
        "speed": 115200
    },
    "url": "https://esphome.io/",
    "vendor": "esphome",
    "build": {
        "bsp": {
            "name": "adafruit"
        },
        "softdevice": {
            "sd_fwid": "0x00B6"
        }
    }
}
"""

CONF_SECOND_BOOTLOADER = "second_bootloader"


def set_core_data(config: ConfigType) -> ConfigType:
    # Resolve toolchain: CLI (already on CORE.toolchain) > YAML > default.
    if CORE.toolchain is None:
        CORE.toolchain = config.get(CONF_TOOLCHAIN, Toolchain.PLATFORMIO)
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR

    if config[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        sections = BOOTLOADER_CONFIG[config[KEY_BOOTLOADER]]
        zephyr_add_pm_static(sections)

        if CONF_SECOND_BOOTLOADER in config and config[CONF_SECOND_BOOTLOADER]:
            # Derive partition addresses from the SoftDevice and bootloader sections so
            # that the DTS flash map matches what the Partition Manager produces:
            #   MCUboot sits immediately after the SoftDevice, then slot0, then slot1.
            mcuboot_size = 0x10000  # 64 KB
            sd_end = next(
                s.address + s.size for s in sections if "SoftDevice" in s.name
            )
            bl_start = next(s.address for s in sections if "Adafruit" in s.name)
            slot0_start = sd_end + mcuboot_size
            # Align slot size down to a 4 KB sector boundary
            slot_size = ((bl_start - slot0_start) // 2 // 0x1000) * 0x1000
            slot1_start = slot0_start + slot_size

            print(f"0x{sd_end:08x} ")
            print(f"0x{slot0_start:08x} ")

            def _mcuboot_partition_overlay() -> str:
                def part(name, start, size):
                    return f"""
                    {name}: partition@{start:x} {{
                        reg = <0x{start:x} 0x{size:x}>;
                    }};"""

                return f"""
                    /delete-node/ &boot_partition;
                    /delete-node/ &storage_partition;
                    /delete-node/ &code_partition;
                    /delete-node/ &reserved_partition_0;

                    &flash0 {{
                        partitions {{
                            compatible = "fixed-partitions";
                            #address-cells = <1>;
                            #size-cells = <1>;
                            {part("slot0_partition", slot0_start, slot_size)}
                            {part("slot1_partition", slot1_start, slot_size)}
                        }};
                    }};
                """

            zephyr_add_overlay(_mcuboot_partition_overlay(), "mcuboot")
            zephyr_add_overlay(_mcuboot_partition_overlay())
            zephyr_add_overlay(
                """
                / {
                    chosen {
                        zephyr,code-partition = &slot0_partition;
                    };
                };
                """
            )
            zephyr_add_overlay(
                """
                / {
                    chosen {
                        zephyr,code-partition = &slot0_partition;
                    };
                };
                """,
                "mcuboot",
            )
    return config


def set_framework(config: ConfigType) -> ConfigType:
    if CONF_VERSION not in config[CONF_FRAMEWORK]:
        default_version = "2.6.1-b" if CORE.using_toolchain_platformio else "2.9.2"
        config = {
            **config,
            CONF_FRAMEWORK: {**config[CONF_FRAMEWORK], CONF_VERSION: default_version},
        }
    framework_ver = cv.Version.parse(
        cv.version_number(config[CONF_FRAMEWORK][CONF_VERSION])
    )
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = framework_ver
    if not CORE.using_toolchain_platformio:
        return config
    if framework_ver < cv.Version(2, 9, 2):
        return cv.require_framework_version(
            nrf52_zephyr=cv.Version(2, 6, 1, "a"),
        )(config)
    if framework_ver < cv.Version(3, 2, 0):
        return cv.require_framework_version(
            nrf52_zephyr=cv.Version(2, 9, 2, "2"),
        )(config)
    return cv.require_framework_version(
        nrf52_zephyr=cv.Version(3, 2, 0, "1"),
    )(config)


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
CONF_SECOND_BOOTLOADER = "second_bootloader"

VOLTAGE_LEVELS = [1.8, 2.1, 2.4, 2.7, 3.0, 3.3]


_DFU_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    }
)


def _dfu_schema(value: bool | ConfigType) -> ConfigType:
    if isinstance(value, bool):
        if not value:
            raise cv.Invalid("Use 'dfu: true' or specify a configuration dict")
        return _DFU_SCHEMA({})
    return _DFU_SCHEMA(value)


CONFIG_SCHEMA = cv.All(
    _detect_bootloader,
    set_core_data,
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.All(
                cv.string_strict, cv.ByteLength(max=BOARD_MAX_LENGTH)
            ),
            cv.Optional(KEY_BOOTLOADER): cv.one_of(*BOOTLOADERS, lower=True),
            cv.Optional(CONF_DFU): _dfu_schema,
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
            cv.Optional(CONF_SECOND_BOOTLOADER, default=False): cv.boolean,
            cv.Optional(
                CONF_FRAMEWORK,
                default={},
            ): cv.Schema(
                {
                    cv.Optional(CONF_VERSION): cv.string_strict,
                    cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
                        {
                            cv.Optional(
                                CONF_ENABLE_OTA_ROLLBACK, default=True
                            ): cv.boolean,
                        }
                    ),
                }
            ),
            cv.GenerateID(CONF_CDC_ACM): cv.declare_id(CdcAcm),
        }
    ),
    set_framework,
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
    full_config = fv.full_config.get()
    conf = config[CONF_FRAMEWORK]
    advanced = conf[CONF_ADVANCED]

    if advanced[CONF_ENABLE_OTA_ROLLBACK]:
        # "disabled: false" means safe mode *is* enabled.
        safe_mode_config = full_config.get(CONF_SAFE_MODE, {CONF_DISABLED: True})
        safe_mode_enabled = not safe_mode_config[CONF_DISABLED]
        ota_enabled = CONF_OTA in full_config
        # Both need to be enabled for rollback to work
        if not (ota_enabled and safe_mode_enabled):
            # But only warn if ota is even possible
            if ota_enabled:
                _LOGGER.warning(
                    "OTA rollback requires safe_mode, disabling rollback support"
                )
            # disable the rollback feature anyway since it can't be used.
            advanced[CONF_ENABLE_OTA_ROLLBACK] = False


FINAL_VALIDATE_SCHEMA = _final_validate


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    # nRF52 processors are single-core
    cg.add_define(ThreadModel.SINGLE)
    if CORE.using_toolchain_platformio:
        cg.add_platformio_option("board", config[CONF_BOARD])
        cg.add_platformio_option(
            CONF_FRAMEWORK, CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK]
        )
        cg.add_platformio_option(
            "platform",
            "https://github.com/tomaszduda23/platform-nordicnrf52/archive/refs/tags/v10.3.0-5.zip",
        )
        cg.add_platformio_option(
            "platform_packages",
            [
                f"platformio/framework-zephyr@https://github.com/tomaszduda23/framework-sdk-nrf/archive/refs/tags/v{CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]}.zip",
            ],
        )
        if config[KEY_BOOTLOADER] != BOOTLOADER_MCUBOOT:
            # make sure that firmware.zip is created
            # for Adafruit_nRF52_Bootloader
            cg.add_platformio_option("board_upload.protocol", "nrfutil")
            cg.add_platformio_option("board_upload.use_1200bps_touch", "true")
            cg.add_platformio_option("board_upload.require_upload_port", "true")
            cg.add_platformio_option("board_upload.wait_for_upload_port", "true")

        add_extra_script(
            "pre",
            "pre_build.py",
            Path(__file__).parent / "pre_build.py.script",
        )
        # build is done by west so bypass board checking in platformio
        cg.add_platformio_option("boards_dir", CORE.relative_build_path("boards"))

    if config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        cg.add_define("USE_BOOTLOADER_MCUBOOT")
    elif "_sd" in config[KEY_BOOTLOADER]:
        bootloader = config[KEY_BOOTLOADER].split("_")
        sd_id = bootloader[2][2:]
        cg.add_define("USE_SOFTDEVICE_ID", int(sd_id))
        if (len(bootloader)) > 3:
            sd_version = bootloader[3][1:]
            cg.add_define("USE_SOFTDEVICE_VERSION", int(sd_version))

    zephyr_setup_preferences()
    zephyr_to_code(config)

    if dfu_config := config.get(CONF_DFU):
        CORE.add_job(_dfu_to_code, dfu_config)
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
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

    conf = config[CONF_FRAMEWORK]
    advanced = conf[CONF_ADVANCED]
    # Enable OTA rollback support
    if advanced[CONF_ENABLE_OTA_ROLLBACK]:
        cg.add_define("USE_OTA_ROLLBACK")
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
    zephyr_add_prj_conf("CONSOLE", False, False)
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
    zephyr_add_prj_conf("REBOOT", True)

    if config[CONF_SECOND_BOOTLOADER]:
        CORE.data[PLATFORM_NRF52] = {"second_bootloader": True}
        # zephyr_add_prj_conf("BOOT_USB_DFU_WAIT", True, image="mcuboot")
        zephyr_add_prj_conf(
            "PM_PARTITION_SIZE_MCUBOOT", HexValue(0x10000), image="mcuboot"
        )
        zephyr_add_prj_conf("BOOT_SERIAL_CDC_ACM", True, image="mcuboot")
        # USB CDC ACM requires multithreading and the full USB device stack
        zephyr_add_prj_conf("MULTITHREADING", True, image="mcuboot")
        zephyr_add_prj_conf("MCUBOOT_LOG_LEVEL_DBG", True, image="mcuboot")
        zephyr_add_prj_conf("LOG_MODE_IMMEDIATE", True, image="mcuboot")
        # zephyr_add_prj_conf("USB_DEVICE_STACK", True, image="mcuboot")
        # zephyr_add_prj_conf("USB_CDC_ACM", True, image="mcuboot")


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _dfu_to_code(dfu_config):
    cg.add_define("USE_NRF52_DFU")
    var = cg.new_Pvariable(dfu_config[CONF_ID])
    if CONF_RESET_PIN in dfu_config:
        pin = await cg.gpio_pin_expression(dfu_config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(pin))
    zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
    await cg.register_component(var, dfu_config)

def copy_files() -> None:
    """Copy files to the build directory."""

    if CORE.using_toolchain_platformio and (
        zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
        or zephyr_data()[KEY_BOARD] == "xiao_ble"
    ):
        write_file_if_changed(
            CORE.relative_build_path(f"boards/{zephyr_data()[KEY_BOARD]}.json"),
            FAKE_BOARD_MANIFEST,
        )

    zephyr_copy_files()

    write_file_if_changed(
        CORE.relative_src_path("CMakeLists.txt"),
        get_component_cmakelists(),
    )


DFU_PATH = "firmware.zip"


def get_download_types(storage_json: StorageJSON) -> list[dict[str, str]]:
    """Get the download types for the firmware."""
    types = []
    UF2_PATH = "zephyr/zephyr.uf2"
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
    from esphome.platformio import toolchain

    if port is not None:
        upload_args += ["--upload-port", port]
    return toolchain.run_platformio_cli_run(config, CORE.verbose, *upload_args)


def upload_program(config: ConfigType, args, host: str) -> bool:
    from esphome.__main__ import PortType, check_permissions, get_port_type

    mcumgr_device: str | None = None

    if (
        get_port_type(host) == PortType.SERIAL
        and config["nrf52"][KEY_BOOTLOADER] != BOOTLOADER_MCUBOOT
    ):
        check_permissions(host)
        if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
            mcumgr_device = host
        else:
            if zephyr_data()[KEY_NATIVE_BUILD]:
                import time

                import serial

                try:
                    if "://" in host:
                        with serial.serial_for_url(host, baudrate=1200):
                            pass
                    else:
                        with serial.Serial(host, baudrate=1200):
                            pass
                except serial.serialutil.SerialException:
                    # It triggers cpu reset. It can fail randomly.
                    pass

                time.sleep(1)

                dfu_zip = CORE.relative_build_path("firmware.zip")
                result = subprocess.run(
                    [
                        "adafruit-nrfutil",
                        "dfu",
                        "serial",
                        "-pkg",
                        str(dfu_zip),
                        "-p",
                        host,
                    ],
                    check=False,
                ).returncode
                if result != 0:
                    raise EsphomeError(f"Upload failed with result: {result}")
                return True  # Handled: native adafruit-nrfutil serial upload
            result = _upload_using_platformio(config, host, ["-t", "upload"])
            if result != 0:
                raise EsphomeError(f"Upload failed with result: {result}")
            return True  # Handled: platformio serial upload

    if host == "PYOCD":
        if zephyr_data()[KEY_NATIVE_BUILD]:
            firmware = _build_env_dir(config[CORE.target_platform]) / "merged.hex"
            result = subprocess.run(
                [
                    "pyocd",
                    "flash",
                    "-t",
                    "nrf52840",
                    str(firmware),
                ],
                check=False,
            ).returncode
            if result != 0:
                raise EsphomeError(f"Upload failed with result: {result}")
        else:
            result = _upload_using_platformio(config, host, ["-t", "flash_pyocd"])
            if result != 0:
                raise EsphomeError(f"Upload failed with result: {result}")
        return True  # Handled: platformio PYOCD upload

    # Deferred imports: bleak/smpclient are heavy, only load for BLE/mcumgr paths
    from .ble_logger import is_mac_address
    from .ota import smpmgr_scan, smpmgr_upload

    if host == "BLE":
        mcumgr_device = asyncio.run(smpmgr_scan(CORE.name))

    if is_mac_address(host):
        mcumgr_device = host

    if mcumgr_device:
        platform_config = config[CORE.target_platform]
        firmware = _firmware_image_path(platform_config).resolve()
        asyncio.run(smpmgr_upload(mcumgr_device, firmware))
        return True  # Handled: mcumgr OTA upload

    return False  # Not handled: let caller try default upload methods


def show_logs(config: ConfigType, args, devices: list[str]) -> bool:
    address = devices[0]
    from .ble_logger import is_mac_address, logger_connect, logger_scan

    if devices[0] == "BLE":
        ble_device = asyncio.run(logger_scan(CORE.name))
        if ble_device:
            address = ble_device.address
        else:
            return True

    if is_mac_address(address):
        asyncio.run(logger_connect(address))
        return True
    return False


def _addr2line(addr2line: str, elf: Path, addr: str) -> str:
    try:
        result = subprocess.run(
            [addr2line, "-e", elf, addr],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout.strip().splitlines()[0]
    except Exception as err:  # pylint: disable=broad-except
        _LOGGER.error("Running command failed: %s", err)
    return ""


def process_stacktrace(config: ConfigType, line: str, backtrace_state: bool) -> bool:
    if "Last crash:" in line:
        return True
    if backtrace_state:
        match = re.search(r"PC=(0x[0-9a-fA-F]+)\s+LR=(0x[0-9a-fA-F]+)", line)
        if match:
            pc = match.group(1)
            lr = match.group(2)
            from esphome.analyze_memory.toolchain import find_tool

            addr2line = find_tool("addr2line")
            if addr2line is None:
                return False
            platform_config = config[CORE.target_platform]
            elf = _elf_path(platform_config)
            if not elf.exists():
                _LOGGER.warning("%s does not exists", elf)
                return False
            _LOGGER.error("=== CRASH ===")
            _LOGGER.error("PC: %s", _addr2line(addr2line, elf, pc))
            _LOGGER.error("LR: %s", _addr2line(addr2line, elf, lr))

    return False


def compile_program(args, config: ConfigType) -> bool:
    platform_config = config[CORE.target_platform]
    if not platform_config[CONF_NATIVE_BUILD]:
        return False  # let PlatformIO handle it

    from .build import run_west_build, write_cmake_lists

    venv_path, sdk_path, toolchain_path = (None, None, None)
    # check_and_install()
    write_cmake_lists()
    rc = run_west_build(
        venv_path=venv_path,
        sdk_path=sdk_path,
        toolchain_path=toolchain_path,
        board=platform_config[CONF_BOARD],
        verbose=CORE.verbose,
        second_bootloader=platform_config[CONF_SECOND_BOOTLOADER],
    )
    if rc != 0:
        raise EsphomeError(f"west build failed with exit code {rc}")

    build_dir = _build_env_dir(platform_config)
    if zephyr_data()[KEY_BOOTLOADER] != BOOTLOADER_MCUBOOT:
        app_bin = build_dir / "merged.hex"
        dfu_zip = CORE.relative_build_path("firmware.zip")
        nrfutil_rc = subprocess.run(
            [
                "adafruit-nrfutil",
                "dfu",
                "genpkg",
                "--dev-type",
                "0x0052",
                "--application",
                str(app_bin),
                str(dfu_zip),
            ],
            check=False,
        ).returncode
        if nrfutil_rc != 0:
            raise EsphomeError(f"adafruit-nrfutil failed with exit code {nrfutil_rc}")
    return True


def run_compile(args, config: ConfigType) -> bool:
    if CORE.using_toolchain_platformio:
        return False
    build(*check_and_install())
    raise EsphomeError("Native build for nRF52 is not implemented yet")
