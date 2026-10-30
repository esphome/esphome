from __future__ import annotations

import asyncio
import logging
from pathlib import Path
import re
import shutil
import subprocess

from esphome import pins
from esphome.build_helpers import pch
from esphome.build_helpers.ccache import resolve_ccache_path
import esphome.codegen as cg
from esphome.components import network
from esphome.components.zephyr import (
    PrjConfValueType,
    Section,
    add_extra_script,
    copy_files as zephyr_copy_files,
    zephyr_add_overlay,
    zephyr_add_pm_static,
    zephyr_add_prj_conf,
    zephyr_add_sysbuild_conf,
    zephyr_data,
    zephyr_set_core_data,
    zephyr_setup_preferences,
    zephyr_to_code,
)
from esphome.components.zephyr.const import (
    BOOTLOADER_MCUBOOT,
    KEY_BOARD,
    KEY_BOOTLOADER,
    KEY_SYSBUILD,
    KEY_ZEPHYR,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_DISABLED,
    CONF_ENABLE_OTA_ROLLBACK,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_INVERTED,
    CONF_NUMBER,
    CONF_OTA,
    CONF_PLATFORM,
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
from esphome.framework_helpers import (
    cmake_escape,
    get_cmake_compile_flags,
    get_cmake_link_flags,
    lex_cmake_build_flags,
    quote_cmake_args,
    run_command_ok,
)
from esphome.helpers import rmtree, write_file_if_changed
from esphome.storage_json import StorageJSON
from esphome.types import ConfigType

from .boards import BOARDS_ZEPHYR, BOOTLOADER_CONFIG, SELF_RESET_PINS
from .const import (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)
from .framework import (
    check_and_install,
    ensure_nordic_nrfutil,
    get_build_env,
    get_build_paths,
    setup_platformio_python_env,
    toolchain_tool,
    wanted_west_projects,
)

# force import gpio to register pin schema
from .gpio import nrf52_pin_to_code  # noqa: F401

CODEOWNERS = ["@tomaszduda23"]
AUTO_LOAD = ["zephyr", "preferences"]
DOMAIN = "nrf52"
IS_TARGET_PLATFORM = True
_LOGGER = logging.getLogger(__name__)

# Default framework versions per toolchain. The sdk-nrf one also keys the CI
# sdk-nrf install cache and pins the clang-tidy project's SDK.
RECOMMENDED_PLATFORMIO_VERSION = "2.6.1-b"
RECOMMENDED_SDK_NRF_VERSION = "2.9.2"

# Flash layout for `bootloader: mcuboot`, where MCUboot owns the device from the
# reset vector. Every nRF52840 has 1 MB, and it is the only nRF52 with the USB
# device that MCUboot's serial recovery needs. A part with different flash makes
# the Partition Manager reject the map at build time rather than quietly
# producing a wrong one, so this is a safe assumption to state here.
NRF52840_FLASH_SIZE = 0x100000
FLASH_SECTOR_SIZE = 0x1000
# Enough for MCUboot with USB-CDC serial recovery and the management groups that
# make a board which will not boot diagnosable over that same connection. The
# build measures around 53 KB; dropping serial recovery would fit half this.
MCUBOOT_PARTITION_SIZE = 0x10000
# Zephyr NVS, behind esphome's preferences.
SETTINGS_PARTITION_SIZE = 0xA000

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


def set_core_data(config: ConfigType) -> ConfigType:
    zephyr_set_core_data(config)
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = PLATFORM_NRF52
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = KEY_ZEPHYR

    if config[KEY_BOOTLOADER] in BOOTLOADER_CONFIG:
        sections = BOOTLOADER_CONFIG[config[KEY_BOOTLOADER]]
        if CORE.testing_mode:
            sections = _testing_mode_sections(sections)
        zephyr_add_pm_static(sections)

    return config


# In testing mode, fake a larger flash to allow linking grouped component
# tests. The nRF52840 has 1 MB and a mcumgr OTA config halves the app slot,
# which an openthread config alone fills; CI images are never flashed.
TESTING_FLASH_SIZE = 0x400000


def _testing_mode_sections(sections: list[Section]) -> list[Section]:
    """Move a bootloader pinned to the end of the real flash to the end of the
    faked one, so the partition manager still sees a single gap for the app."""
    return [
        Section(
            section.name,
            section.address + TESTING_FLASH_SIZE - NRF52840_FLASH_SIZE,
            section.size,
            section.region,
        )
        if section.end_address == NRF52840_FLASH_SIZE
        else section
        for section in sections
    ]


_TOOLCHAINS = (Toolchain.PLATFORMIO, Toolchain.SDK_NRF)
_resolve_toolchain = cv.resolve_toolchain("nRF52", _TOOLCHAINS, Toolchain.SDK_NRF)


def set_framework(config: ConfigType) -> ConfigType:
    if CONF_VERSION not in config[CONF_FRAMEWORK]:
        default_version = (
            RECOMMENDED_PLATFORMIO_VERSION
            if CORE.using_toolchain_platformio
            else RECOMMENDED_SDK_NRF_VERSION
        )
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


# Bootloaders flashed over USB serial DFU: the Adafruit ones with adafruit-nrfutil, the factory Open
# DFU bootloader with Nordic's nrfutil; mcuboot uses mcumgr instead
SERIAL_DFU_BOOTLOADERS = (
    BOOTLOADER_ADAFRUIT,
    BOOTLOADER_ADAFRUIT_NRF52_SD132,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V6,
    BOOTLOADER_ADAFRUIT_NRF52_SD140_V7,
    BOOTLOADER_NRF,
)
BOOTLOADERS = [*SERIAL_DFU_BOOTLOADERS, BOOTLOADER_MCUBOOT]


_validate_toolchain = cv.toolchain_enum(_TOOLCHAINS)


def _detect_bootloader(config: ConfigType) -> ConfigType:
    """Detect the bootloader for the given board."""
    config = config.copy()
    bootloaders: list[str] = []
    if CONF_BOARD not in config:
        raise cv.Invalid("'board' is a required option for [nrf52].")
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
CONF_LIBC_NANO = "libc_nano"
CONF_REG0 = "reg0"
CONF_UICR_ERASE = "uicr_erase"

VOLTAGE_LEVELS = [1.8, 2.1, 2.4, 2.7, 3.0, 3.3]


_DFU_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeviceFirmwareUpdate),
        cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
    }
)


def _default_self_reset_pin(config: ConfigType) -> ConfigType:
    # The factory bootloader ignores the retained register the touch sets, so a board with a GPIO
    # wired to RESET resets itself through that pin
    dfu = config.get(CONF_DFU)
    number = SELF_RESET_PINS.get(config[CONF_BOARD])
    if dfu is not None and number and CONF_RESET_PIN not in dfu:
        dfu[CONF_RESET_PIN] = pins.gpio_output_pin_schema(
            {CONF_NUMBER: number, CONF_INVERTED: True}
        )
    return config


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
            cv.Optional(CONF_DCDC): cv.boolean,
            cv.Optional(CONF_REG0): cv.Schema(
                {
                    cv.Required(CONF_VOLTAGE): cv.All(
                        cv.voltage,
                        cv.one_of(*VOLTAGE_LEVELS, float=True),
                    ),
                    cv.Optional(CONF_UICR_ERASE, default=False): cv.boolean,
                }
            ),
            cv.Optional(
                CONF_FRAMEWORK,
                default={},
            ): cv.Schema(
                {
                    cv.Optional(CONF_VERSION): cv.string_strict,
                    cv.Optional(CONF_LIBC_NANO): cv.boolean,
                    cv.Optional(
                        CONF_ADVANCED, default={}, visibility=cv.Visibility.YAML_ONLY
                    ): cv.Schema(
                        {
                            # Defaults to whether MCUboot is present, see _final_validate
                            cv.Optional(CONF_ENABLE_OTA_ROLLBACK): cv.boolean,
                        }
                    ),
                }
            ),
            cv.Optional(CONF_TOOLCHAIN): _validate_toolchain,
        }
    ),
    _default_self_reset_pin,
    _resolve_toolchain,
    set_framework,
    network.require_ipv6(PLATFORM_NRF52),
    network.request_ipv4_off(PLATFORM_NRF52),
)


def _validate_mcumgr(config):
    bootloader = zephyr_data()[KEY_BOOTLOADER]
    if bootloader == BOOTLOADER_MCUBOOT:
        raise cv.Invalid(f"'{bootloader}' bootloader does not support DFU")


def _validate_open_dfu_toolchain(config: ConfigType) -> None:
    # The deprecated PlatformIO toolchain configures every non-mcuboot upload for a touch-reset
    # bootloader, which the factory Open DFU bootloader is not
    if config[KEY_BOOTLOADER] == BOOTLOADER_NRF and CORE.using_toolchain_platformio:
        raise cv.Invalid(
            f"'{BOOTLOADER_NRF}' bootloader requires 'toolchain: {Toolchain.SDK_NRF}'"
        )


def _final_validate(config):

    # Remove before 2027.2.0
    if CORE.using_toolchain_platformio:
        _LOGGER.warning(
            "The 'platformio' toolchain for nRF52 is deprecated and will be removed in ESPHome 2027.2.0. "
            "Please use 'toolchain: sdk-nrf' instead."
        )
    _validate_open_dfu_toolchain(config)

    if CONF_DFU in config:
        _validate_mcumgr(config)
    if _mcuboot_replaces_factory_bootloader(config) and CORE.data[KEY_CORE][
        KEY_FRAMEWORK_VERSION
    ] < cv.Version(2, 9, 2):
        # The MCUboot image is configured through sysbuild, which replaced the
        # zephyr/child_image/ mechanism in NCS 2.9.2. On an older SDK the
        # fragments are silently ignored and the board comes out with no
        # bootloader at all, so refuse the config instead.
        raise cv.Invalid(
            f"'{BOOTLOADER_MCUBOOT}' requires nRF Connect SDK 2.9.2 or newer. "
            "Use 'toolchain: sdk-nrf', or set 'framework: version:' to 2.9.2 "
            "or later."
        )
    if config[KEY_BOOTLOADER] == BOOTLOADER_ADAFRUIT:
        _LOGGER.warning(
            "Selected generic Adafruit bootloader. The board might crash. Consider settings `bootloader:`"
        )
    full_config = fv.full_config.get()
    conf = config[CONF_FRAMEWORK]
    advanced = conf[CONF_ADVANCED]

    if conf.get(CONF_LIBC_NANO, False) and "logger" in CORE.loaded_integrations:
        _LOGGER.warning(
            "Logger is enabled with newlib-nano (libc_nano: true). Some format specifiers "
            "such as %%zu are not supported and will print incorrectly. "
            "Set 'libc_nano: false' under 'framework:' to use the full newlib."
        )

    # Rollback confirms a freshly flashed image through MCUboot, which is either
    # the bootloader or, for zephyr_mcumgr, a second stage after it.
    mcuboot = (
        config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
        or "zephyr_mcumgr" in CORE.loaded_integrations
    )
    if advanced.get(CONF_ENABLE_OTA_ROLLBACK) and not mcuboot:
        # Older configs set it explicitly; it never had an effect without MCUboot
        _LOGGER.warning(
            "'%s' requires MCUboot, which the '%s' bootloader does not have; ignoring it",
            CONF_ENABLE_OTA_ROLLBACK,
            config[KEY_BOOTLOADER],
        )
        advanced[CONF_ENABLE_OTA_ROLLBACK] = False
    if advanced.setdefault(CONF_ENABLE_OTA_ROLLBACK, mcuboot):
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


def _read_kconfig_fragment(path: Path) -> list[tuple[str, PrjConfValueType]]:
    """Parse the NAME=value lines of a Kconfig fragment."""
    options: list[tuple[str, PrjConfValueType]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        name, raw = line.split("=", 1)
        value: PrjConfValueType
        if raw in ("y", "n"):
            value = raw == "y"
        elif raw.startswith('"'):
            value = raw.strip('"')
        else:
            value = int(raw, 0)
        options.append((name, value))
    return options


def _mcuboot_replaces_factory_bootloader(config: ConfigType) -> bool:
    """MCUboot was selected on a board that ships with a factory bootloader.

    Boards without a bootloader table default to MCUboot and use the MCUboot
    layout of their own devicetree; only boards with a factory bootloader
    layout need the flash map from _mcuboot_to_code().
    """
    return (
        config[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
        and config[CONF_BOARD] in BOARDS_ZEPHYR
    )


def _mcuboot_to_code() -> None:
    """Configure a two-slot MCUboot that owns the device from the reset vector.

    This *replaces* any factory bootloader rather than chaining behind it, so
    the board is flashed once over SWD with merged.hex and every update after
    that is an MCUboot image upload (BLE or USB-CDC serial recovery). In
    exchange there is no SoftDevice reservation, the whole part is available,
    and nothing on the device ever rewrites UICR.

    The flash map is derived from the partition sizes above rather than written
    out per board. On a 1 MB nRF52840 it comes out as:

        mcuboot    0x00000..0x10000    64 KiB
        primary    0x10000..0x83000   460 KiB   <- the gap
        secondary  0x83000..0xF6000   460 KiB
        settings   0xF6000..0x100000   40 KiB

    Only the primary slot is left implicit: the Partition Manager fills the
    single gap with it. Everything else is stated, because left to itself PM
    sizes the two application slots first and hands settings_storage whatever
    remains -- a single 0x2000, the bare two sectors Zephyr NVS will accept.
    """
    flash_size = TESTING_FLASH_SIZE if CORE.testing_mode else NRF52840_FLASH_SIZE
    settings_address = flash_size - SETTINGS_PARTITION_SIZE
    slots_end = settings_address
    if "zephyr_coredump" in CORE.loaded_integrations:
        # zephyr_coredump places its partition directly below settings_storage
        from esphome.components.zephyr_coredump import PARTITION_SIZE

        slots_end -= PARTITION_SIZE
    slot_size = (slots_end - MCUBOOT_PARTITION_SIZE) // 2
    slot_size -= slot_size % FLASH_SECTOR_SIZE
    slot0_address = MCUBOOT_PARTITION_SIZE
    slot1_address = slot0_address + slot_size

    # Build the app as an MCUboot image (signed, linked into the primary slot)
    # and pull in the MCUboot sysbuild image. The bootloader selection has to do
    # this itself: a config with no zephyr_mcumgr OTA still needs a bootloader
    # to boot at all, and without it the app links as a plain application at 0x0
    # and MCUboot is never built.
    zephyr_add_prj_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_data()[KEY_SYSBUILD] = True

    # Board defconfigs turn on UF2 output for the Adafruit bootloader, which
    # consumes it. There is no Adafruit bootloader here, and the UF2 that would
    # be emitted carries the app's new load address rather than the one the
    # factory bootloader expects, so it is actively misleading. Dropping it also
    # lets get_download_types() offer merged.hex and app_update.bin instead.
    zephyr_add_prj_conf("BUILD_OUTPUT_UF2", False)

    zephyr_add_pm_static(
        [
            Section("mcuboot", 0x0, MCUBOOT_PARTITION_SIZE, "flash_primary"),
            Section("mcuboot_secondary", slot1_address, slot_size, "flash_primary"),
            Section(
                "settings_storage",
                settings_address,
                SETTINGS_PARTITION_SIZE,
                "flash_primary",
            ),
        ]
    )

    # Hash-only image validation (signature type "none"). ECDSA-P256, the
    # nRF52840 sysbuild default, does fit this partition -- but the key it signs
    # with is MCUboot's published sample key, which every copy of the SDK ships.
    # Verifying against a keypair anyone can obtain buys no authenticity, so
    # rather than imply a guarantee that isn't there, check integrity only and
    # say so. Real signing needs a user-supplied key, which is worth adding as
    # an explicit option rather than defaulting to a public one.
    #
    # Under sysbuild the signature type is chosen at the sysbuild level: the
    # mcuboot image inherits it and the app is signed to match.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_NONE", True)
    # Overwrite-only upgrades: no swap, no revert, no trailer state, so a board
    # cannot get stuck re-running a revert (an MCUboot v2.1.0 defect fixed only
    # in v2.2.0, which is newer than the SDK this builds against). The trade is
    # no automatic rollback; recovery is USB-CDC serial recovery, or SWD.
    #
    # Both of these MUST be set at the sysbuild level. The upgrade mode is a
    # Kconfig *choice*, and sysbuild force-writes every member of that choice
    # into the image's FORCED_CONF_FILE -- so CONFIG_BOOT_UPGRADE_ONLY=y in the
    # image's own fragment is applied and then silently overridden back to n.
    zephyr_add_sysbuild_conf("MCUBOOT_MODE_OVERWRITE_ONLY", True)

    # Deliver the MCUboot Kconfig fragment and devicetree overlay to the MCUboot
    # sysbuild image. The fragment goes through the image's prj_conf, which
    # zephyr writes to the same zephyr/sysbuild/mcuboot.conf together with the
    # options other components set for MCUboot (the zephyr_mcumgr status LED).
    for name, value in _read_kconfig_fragment(Path(__file__).parent / "mcuboot.conf"):
        zephyr_add_prj_conf(name, value, image="mcuboot")
    zephyr_add_overlay(
        (Path(__file__).parent / "mcuboot.overlay").read_text(),
        image="mcuboot",
    )

    # slot0_partition / slot1_partition devicetree labels. MCUboot's CMakeLists
    # reads erase-block-size / write-block-size and the sector count from these
    # at configure time, and NCS's image-signing step reads slot0_partition
    # (REQUIRED) to size the signed app image -- so both images need them.
    # Board DTS files ship a factory-bootloader layout and define neither, and
    # without them the devicetree configure step fails outright.
    mcuboot_slots = f"""
        &flash0 {{
            partitions {{
                slot0_partition: partition@{slot0_address:x} {{
                    label = "image-0";
                    reg = <0x{slot0_address:08x} 0x{slot_size:08x}>;
                }};
                slot1_partition: partition@{slot1_address:x} {{
                    label = "image-1";
                    reg = <0x{slot1_address:08x} 0x{slot_size:08x}>;
                }};
            }};
        }};
    """
    zephyr_add_overlay(mcuboot_slots, image="mcuboot")
    zephyr_add_overlay(mcuboot_slots, image="")


@coroutine_with_priority(CoroPriority.PLATFORM)
async def to_code(config: ConfigType) -> None:
    """Convert the configuration to code."""
    cg.add_build_flag("-DUSE_NRF52")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "NRF52")
    # Zephyr is a preemptive RTOS: BLE, Thread, USB and workqueue callbacks
    # run in other threads, so the scheduler must be thread-safe.
    if config[CONF_BOARD] == "bbc_microbit":
        # micro:bit v1 is an nRF51822 (Cortex-M0, ARMv6-M) without LDREX/STREX;
        # std::atomic RMW would need libatomic, so use k_mutex-based locking.
        cg.add_define(ThreadModel.MULTI_NO_ATOMICS)
    else:
        # nRF52 (Cortex-M4) has LDREX/STREX
        cg.add_define(ThreadModel.MULTI_ATOMICS)
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
        if _mcuboot_replaces_factory_bootloader(config):
            _mcuboot_to_code()
    elif "_sd" in config[KEY_BOOTLOADER]:
        bootloader = config[KEY_BOOTLOADER].split("_")
        sd_id = bootloader[2][2:]
        cg.add_define("USE_SOFTDEVICE_ID", int(sd_id))
        if (len(bootloader)) > 3:
            sd_version = bootloader[3][1:]
            cg.add_define("USE_SOFTDEVICE_VERSION", int(sd_version))

    zephyr_setup_preferences()
    zephyr_to_code(config)

    if CORE.testing_mode:
        zephyr_add_overlay(
            f"""
                &flash0 {{
                    reg = <0x0 {TESTING_FLASH_SIZE:#x}>;
                }};
            """
        )

    if dfu_config := config.get(CONF_DFU):
        CORE.add_job(_dfu_to_code, dfu_config)
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if CONF_DCDC in config:
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
    zephyr_add_prj_conf("NEWLIB_LIBC", True)
    zephyr_add_prj_conf("NEWLIB_LIBC_FLOAT_PRINTF", True)
    zephyr_add_prj_conf(
        "NEWLIB_LIBC_NANO",
        conf.get(CONF_LIBC_NANO, "logger" not in CORE.loaded_integrations),
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
    if config[CONF_BOARD] == "nrf52840dongle":
        # The board DTS enables uart0 on edge GPIOs (P0.17, P0.22, P0.24) although nothing is wired to it
        zephyr_add_overlay(
            """
                &uart0 {
                    status = "disabled";
                };
            """
        )
    zephyr_add_prj_conf("REBOOT", True)

    # some boards enable USB and UART by default.
    # disable it to prevent extra current consumption.
    zephyr_add_prj_conf("USB_DEVICE_STACK", False, False)
    zephyr_add_prj_conf("SERIAL", False, False)

    # disable stuff to make image smaller by default
    if framework_ver >= cv.Version(2, 9, 2):
        zephyr_add_prj_conf("NCS_BOOT_BANNER", False, False)
    zephyr_add_prj_conf("BOOT_BANNER", False, False)
    zephyr_add_prj_conf("PRINTK", False, False)
    zephyr_add_prj_conf("CONSOLE", False, False)
    zephyr_add_prj_conf("UART_CONSOLE", False)


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _dfu_to_code(dfu_config):
    cg.add_define("USE_NRF52_DFU")
    var = cg.new_Pvariable(dfu_config[CONF_ID])
    await cg.add_pin(var.set_reset_pin, dfu_config.get(CONF_RESET_PIN))

    # DFU uses cdc rate callback to enter bootloader which was disabled explicitly to save power.
    zephyr_add_prj_conf("USB_DEVICE_STACK", True)
    zephyr_add_prj_conf("USB_CDC_ACM", True)
    zephyr_add_prj_conf("CDC_ACM_DTE_RATE_CALLBACK_SUPPORT", True)
    zephyr_add_prj_conf("SERIAL", True)
    await cg.register_component(var, dfu_config)


def copy_files() -> None:
    """Copy files to the build directory."""

    # Library conversion to Zephyr modules is wired into the sdk-nrf
    # CMakeLists only; the PlatformIO toolchain's forked platform package
    # cannot compile external libraries at all, so the build would fail at
    # link time anyway. Fail fast with a clear message instead.
    if CORE.using_toolchain_platformio and CORE.platformio_libraries:
        raise EsphomeError(
            f"Libraries ({', '.join(sorted(CORE.platformio_libraries))}) are "
            "not supported on the nRF52 'platformio' toolchain; use toolchain "
            "'sdk-nrf' to build them as Zephyr modules."
        )

    if CORE.using_toolchain_platformio and (
        zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT
        or zephyr_data()[KEY_BOARD] == "xiao_ble"
    ):
        write_file_if_changed(
            CORE.relative_build_path(f"boards/{zephyr_data()[KEY_BOARD]}.json"),
            FAKE_BOARD_MANIFEST,
        )

    zephyr_copy_files()


def get_download_types(storage_json: StorageJSON) -> list[dict[str, str]]:
    """Get the download types for the firmware."""
    # No recorded firmware path means nothing was built; no downloads.
    if storage_json.firmware_bin_path is None:
        return []
    types = []
    UF2_PATH = "zephyr/zephyr.uf2"
    HEX_PATH = "zephyr/zephyr.hex"  # SDK 2.6.1 without OTA, SDK 3.4.0+
    HEX_MERGED_PATH = "zephyr/merged.hex"  # SDK 2.9.2 to 3.3.x, always generated
    APP_IMAGE_PATH = "zephyr/app_update.bin"
    build_dir = Path(storage_json.firmware_bin_path).parent
    has_uf2 = (build_dir / UF2_PATH).is_file()
    if has_uf2:
        types = [
            {
                "title": "UF2 package (recommended)",
                "description": "For flashing via Adafruit nRF52 Bootloader as a flash drive.",
                "file": UF2_PATH,
                "download": f"{storage_json.name}.uf2",
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
    # Bootloaders without a UF2 family (the dongle's Open DFU bootloader) still get a DFU package
    if (build_dir / DFU_PACKAGE).is_file():
        types += [
            {
                "title": "DFU package",
                "description": "For flashing via nrfutil using USB CDC.",
                "file": DFU_PACKAGE,
                "download": f"dfu-{storage_json.name}.zip",
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

    setup_platformio_python_env()
    if port is not None:
        upload_args += ["--upload-port", port]
    return toolchain.run_platformio_cli_run(config, CORE.verbose, *upload_args)


# USB vendor and product id the nRF52840 Dongle enumerates with while its Open DFU bootloader runs
OPEN_DFU_USB_ID = (0x1915, 0x521F)
DFU_PACKAGE = "firmware.zip"  # the serial DFU package, next to the firmware in the build directory


def _reset_into_bootloader(host: str) -> None:
    """Send the 1200 bps touch that resets an Adafruit bootloader into DFU mode."""
    import time as _time

    import serial as _serial
    import serial.tools.list_ports as _list_ports

    try:
        ser = _serial.Serial(host, baudrate=1200, timeout=1)
        ser.close()
    except _serial.SerialException as err:
        raise EsphomeError(f"Failed to open {host}: {err}") from err

    # Wait for device to reset (port disappears)
    deadline = _time.monotonic() + 5
    while _time.monotonic() < deadline:
        _time.sleep(0.1)
        if host not in {p.device for p in _list_ports.comports()}:
            break
    else:
        _LOGGER.warning(
            "Device did not leave %s within 5 s; it may not have entered bootloader mode",
            host,
        )

    # Wait for DFU port to reappear
    deadline = _time.monotonic() + 10
    while _time.monotonic() < deadline:
        _time.sleep(0.1)
        if host in {p.device for p in _list_ports.comports()}:
            break
    else:
        raise EsphomeError(
            f"DFU port {host!r} did not reappear within 10 s. "
            "Check that the device entered DFU mode."
        )


def _wait_for_port_permissions(host: str) -> None:
    """Give udev a moment to set up the re-enumerated port before using it."""
    import time as _time

    from esphome.__main__ import check_permissions

    deadline = _time.monotonic() + 5
    while _time.monotonic() < deadline:
        try:
            check_permissions(host)
            return
        except EsphomeError:
            _time.sleep(0.05)
    check_permissions(host)  # raises with helpful message


def _open_dfu_ports() -> set[str]:
    import serial.tools.list_ports as _list_ports

    return {
        port.device
        for port in _list_ports.comports(include_links=True)
        if (port.vid, port.pid) == OPEN_DFU_USB_ID
    }


def _enter_open_dfu(host: str) -> str:
    """The port of the Open DFU bootloader: ``host`` when it is one already, otherwise the one that
    appears after the 1200 bps touch reboots an application built with ``dfu:`` into it. The
    bootloader is its own USB device, so it never comes back under the application's path, and
    only a port that was not there before the touch counts, not another dongle's bootloader."""
    import time as _time

    import serial as _serial

    before = _open_dfu_ports()
    if host in before:
        return host
    try:
        _serial.Serial(host, baudrate=1200, timeout=1).close()
    except _serial.SerialException as err:
        raise EsphomeError(f"Failed to open {host}: {err}") from err
    deadline = _time.monotonic() + 10
    while _time.monotonic() < deadline:
        _time.sleep(0.1)
        if appeared := _open_dfu_ports() - before:
            port = min(appeared)
            _wait_for_port_permissions(port)
            return port
    raise EsphomeError(
        f"{host} did not enter the Open DFU bootloader. An application built with 'dfu:' enters it "
        "on upload; otherwise press the RESET button on the side of the dongle (the red LED pulses "
        "while the bootloader runs) and upload again."
    )


def upload_program(config: ConfigType, args, host: str) -> bool:
    from esphome.__main__ import check_permissions
    from esphome.upload_targets import PortType, get_port_type

    if KEY_ZEPHYR not in CORE.data:
        platform_config = config.get(CORE.target_platform)
        if not platform_config:
            raise EsphomeError(
                "nRF52 platform configuration is missing; "
                "please re-validate and recompile."
            )
        set_core_data(platform_config)
        set_framework(platform_config)

    mcumgr_device: str | None = None

    if get_port_type(host) == PortType.SERIAL:
        check_permissions(host)
        bootloader = zephyr_data()[KEY_BOOTLOADER]
        if bootloader == BOOTLOADER_MCUBOOT:
            mcumgr_device = host
        elif CORE.using_toolchain_platformio:
            result = _upload_using_platformio(config, host, ["-t", "upload"])
            if result != 0:
                raise EsphomeError(f"Upload failed with result: {result}")
            return True
        elif bootloader not in SERIAL_DFU_BOOTLOADERS:
            raise EsphomeError(
                f"Serial upload is not implemented for the {bootloader} bootloader"
            )
        else:
            env = get_build_env(None)  # no compile, just the DFU tool
            dfu_package = CORE.relative_pioenvs_path(CORE.name) / DFU_PACKAGE
            if not dfu_package.is_file():
                raise EsphomeError("Firmware not found. Please compile first.")
            if bootloader == BOOTLOADER_NRF:
                # The factory Open DFU bootloader speaks Nordic's Secure DFU protocol
                dfu_cmd = [
                    ensure_nordic_nrfutil(env),
                    "nrf5sdk-tools",
                    "dfu",
                    "usb-serial",
                    "-pkg",
                    str(dfu_package),
                    "-p",
                    _enter_open_dfu(host),
                ]
            else:
                check_and_install()
                paths = get_build_paths()
                _reset_into_bootloader(host)
                _wait_for_port_permissions(host)
                dfu_cmd = [
                    str(paths["python_executable"]),
                    "-m",
                    "nordicsemi.__main__",
                    "dfu",
                    "serial",
                    "-pkg",
                    str(dfu_package),
                    "-p",
                    host,
                    "-b",
                    "115200",
                    "--singlebank",
                ]
            if not run_command_ok(dfu_cmd, env=env, stream_output=True):
                raise EsphomeError("nRF52 serial DFU upload failed")
            return True

    if host == "PYOCD":
        if not CORE.using_toolchain_platformio:
            check_and_install()
            paths = get_build_paths()
            env = get_build_env(resolve_ccache_path())  # west flash may rebuild
            build_dir = CORE.relative_pioenvs_path(CORE.name)
            west_cmd = [
                str(paths["python_executable"]),
                "-m",
                "west",
                "flash",
                "--runner",
                "pyocd",
                "-d",
                str(build_dir),
            ]
            if not run_command_ok(
                west_cmd,
                env=env,
                stream_output=True,
                cwd=str(paths["framework_path"]),
            ):
                raise EsphomeError("nRF52 pyocd flash failed")
        else:
            result = _upload_using_platformio(config, host, ["-t", "flash_pyocd"])
            if result != 0:
                raise EsphomeError(f"Upload failed with result: {result}")
        return True  # Handled: PYOCD upload

    if mcumgr_device:
        # MCUboot over USB CDC: the serial port speaks the MCUmgr SMP protocol
        asyncio.run(_smpmgr_upload(mcumgr_device))
        return True  # Handled: mcumgr serial upload

    # Deferred imports: bleak and friends are heavy, only load for BLE paths
    from .ble_logger import is_ble_address

    if host == "BLE" or is_ble_address(host):
        asyncio.run(_upload_over_ble(config, host))
        return True  # Handled: BLE OTA upload

    return False  # Not handled: let caller try default upload methods


async def _smpmgr_upload(device: str) -> None:
    """Upload the MCUboot application image over the MCUmgr SMP protocol."""
    from .ota import smpmgr_upload

    firmware = Path(
        CORE.relative_pioenvs_path(CORE.name, "zephyr", "app_update.bin")
    ).resolve()
    await smpmgr_upload(device, firmware)


async def _upload_over_ble(config: ConfigType, host: str) -> None:
    """Upload over the OTA protocol the firmware was built with."""
    from .ota import smpmgr_scan

    # Read the config, not CORE.loaded_integrations: the validated config cache
    # skips validation, which is what fills loaded_integrations.
    if not any(
        ota.get(CONF_PLATFORM) == "adafruit_ble" for ota in config.get(CONF_OTA, [])
    ):
        address = await smpmgr_scan(CORE.name) if host == "BLE" else host
        await _smpmgr_upload(address)
        return

    from esphome.components.adafruit_ble import dfu

    # A broken package must not leave a working device waiting in its bootloader
    package = dfu.load_firmware(CORE.relative_pioenvs_path(CORE.name) / DFU_PACKAGE)
    address = host
    if host == "BLE":
        try:
            address = await smpmgr_scan(CORE.name)
        except EsphomeError as err:
            raise EsphomeError(f"{err}. {dfu.BOOTLOADER_HINT}") from err
    bootloader_address = await dfu.enter_dfu_mode(address)
    await dfu.dfu_upload(bootloader_address, package)


def show_logs(config: ConfigType, args, devices: list[str]) -> bool:
    address = devices[0]
    from .ble_logger import is_ble_address, logger_connect, logger_scan

    if devices[0] == "BLE":
        ble_device = asyncio.run(logger_scan(CORE.name))
        if ble_device:
            address = ble_device.address
        else:
            return True

    if is_ble_address(address):
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
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-except
        _LOGGER.error("Running command failed: %s", err)
    return ""


# The PC bound matches the gate in platform_hooks.STACKTRACE_GATES;
# the logger prints both registers with %08x, so a real PC is always
# 8 digits. tests/unit_tests/test_stacktrace.py guards against drift.
STACKTRACE_NRF52_PC_LR_RE = re.compile(r"PC=(0x[0-9a-fA-F]{3,})\s+LR=(0x[0-9a-fA-F]+)")


def process_stacktrace(config: ConfigType, line: str, backtrace_state: bool) -> bool:
    if "Last crash:" in line:
        return True
    if backtrace_state:
        match = STACKTRACE_NRF52_PC_LR_RE.search(line)
        if match:
            pc = match.group(1)
            lr = match.group(2)
            from esphome.analyze_memory.toolchain import find_tool

            addr2line = find_tool("addr2line")
            if addr2line is None:
                return False

            candidates = [
                CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr", "zephyr.elf"),
                CORE.relative_pioenvs_path(CORE.name, "zephyr", "zephyr.elf"),
                CORE.relative_pioenvs_path(CORE.name, "firmware.elf"),
            ]

            elf = next((path for path in candidates if path.exists()), None)

            if elf is None:
                _LOGGER.warning(
                    "None of the expected ELF files exist:\n%s",
                    "\n".join(str(p) for p in candidates),
                )
                return False

            _LOGGER.error("=== CRASH ===")
            _LOGGER.error("PC: %s", _addr2line(addr2line, elf, pc))
            _LOGGER.error("LR: %s", _addr2line(addr2line, elf, lr))

    return False


# GCC only loads a precompiled header ahead of every other forced header, and
# Zephyr forces two with -imacros. They hold macros only, so the C++ sources
# of the app get them through the precompiled header.
_PCH_CMAKE_LINES = [
    "",
    "# ESPHome precompiled header",
    "get_property(esphome_options TARGET zephyr_interface",
    "    PROPERTY INTERFACE_COMPILE_OPTIONS)",
    "set(esphome_kept_options)",
    "set(esphome_pch_headers)",
    "foreach(option IN LISTS esphome_options)",
    '  if(option MATCHES "imacros> ([^>]+)")',
    '    list(APPEND esphome_pch_headers "${CMAKE_MATCH_1}")',
    "    list(APPEND esphome_kept_options",
    '        "$<$<NOT:$<AND:$<COMPILE_LANGUAGE:CXX>,$<STREQUAL:$<TARGET_PROPERTY:NAME>,app>>>:${option}>")',
    "  else()",
    '    list(APPEND esphome_kept_options "${option}")',
    "  endif()",
    "endforeach()",
    "if(NOT esphome_pch_headers)",
    '  message(FATAL_ERROR "ESPHome: the headers Zephyr forces were not found, so "',
    '      "the precompiled header would not load (set ESPHOME_PCH_ENABLE=0)")',
    "endif()",
    "set_property(TARGET zephyr_interface",
    '    PROPERTY INTERFACE_COMPILE_OPTIONS "${esphome_kept_options}")',
    *(
        f'list(APPEND esphome_pch_headers "${{CMAKE_CURRENT_LIST_DIR}}/../src/{header}")'
        for header in pch.PCH_DEFAULT_HEADERS
    ),
    'list(TRANSFORM esphome_pch_headers REPLACE "(.+)" "$<$<COMPILE_LANGUAGE:CXX>:\\\\1>")',
    "target_precompile_headers(app PRIVATE ${esphome_pch_headers})",
]
# Where CMake puts the .gch of the app, below its binary dir
_PCH_SUM_PATH = "CMakeFiles/app.dir/cmake_pch.hxx.gch.sum"


def _write_pch_checksum(build_dir: Path, source_dir: Path) -> None:
    """Write the checksum ccache reads in place of the .gch; before the
    first build too, or its compiles hash the path laden .gch instead.
    The app image dir follows the SDK version, like get_elf_path;
    2.9.2+ always wraps the build in sysbuild."""
    app_dir = build_dir
    if CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] >= cv.Version(2, 9, 2):
        app_dir = build_dir / "zephyr"
    checksum = pch.pch_checksum(
        CORE.relative_src_path(),
        pch.PCH_DEFAULT_HEADERS,
        (
            str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
            zephyr_data()[KEY_BOARD],
            # Kconfig inputs that reach autoconf.h without a .conf line
            ",".join(sorted(wanted_west_projects())),
            str(zephyr_data().get(KEY_SYSBUILD)),
            # What the Zephyr configuration is generated from
            *(
                path.read_text(encoding="utf-8")
                for path in sorted(source_dir.iterdir())
                if path.suffix in (".conf", ".overlay")
            ),
        ),
    )
    write_file_if_changed(app_dir / _PCH_SUM_PATH, checksum + "\n")


def _pch_usable() -> bool:
    return pch.pch_usable((toolchain_tool("g++"),))


def _generate_cmake_lists(pch_on: bool) -> bool:
    """Write the project CMakeLists.txt, returning True if it changed."""
    entries = lex_cmake_build_flags("nRF52")
    compile_flags = get_cmake_compile_flags(entries)
    link_flags = get_cmake_link_flags(entries)

    # Convert any PlatformIO libraries added via cg.add_library() into Zephyr
    # modules and discover them through EXTRA_ZEPHYR_MODULES (a CMake list, set
    # before find_package(Zephyr) so the modules are picked up). Only
    # framework-agnostic libraries actually compile under Zephyr.
    from esphome.components.zephyr.library import generate_zephyr_modules

    module_dirs = generate_zephyr_modules(list(CORE.platformio_libraries.values()))

    lines = [
        "cmake_minimum_required(VERSION 3.20.0)",
        "",
        'set(Zephyr_DIR "$ENV{ZEPHYR_BASE}/share/zephyr-package/cmake/")',
        "",
    ]

    if module_dirs:
        modules = ";".join(str(d).replace("\\", "/") for d in module_dirs)
        lines += [f'set(EXTRA_ZEPHYR_MODULES "{modules}")', ""]

    lines += [
        "find_package(Zephyr REQUIRED)",
        "",
        f"project({CORE.name})",
        "",
        'file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")',
        "",
        "target_sources(app PRIVATE ${APP_SOURCES})",
        'target_include_directories(app PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")',
    ]

    defines = [flag for flag in compile_flags if flag.startswith("-D")]
    options = [flag for flag in compile_flags if not flag.startswith("-D")]
    if defines:
        lines += [
            "",
            "target_compile_options(app PRIVATE",
            *[f"  {arg}" for arg in quote_cmake_args(defines)],
            ")",
        ]
    if options:
        # A target's own options come before the ones it inherits from zephyr_interface, and the
        # compiler keeps the last of two opposing warning flags. Source options come after both.
        lines += [
            "",
            "set_source_files_properties(${APP_SOURCES} PROPERTIES",
            f'  COMPILE_OPTIONS "{";".join(cmake_escape(flag) for flag in options)}"',
            ")",
        ]

    if pch_on:
        lines += _PCH_CMAKE_LINES

    if link_flags:
        lines += [
            "",
            "zephyr_ld_options(",
            *[f"  {arg}" for arg in quote_cmake_args(link_flags)],
            ")",
        ]

    return write_file_if_changed(
        CORE.relative_build_path("zephyr", "CMakeLists.txt"),
        "\n".join(lines) + "\n",
    )


def _copy_if_exists(src: Path, dst: Path) -> None:
    if src.is_file():
        shutil.copy2(src, dst)


def _west_build_command(
    python_executable: Path, board: str, build_dir: Path, source_dir: Path
) -> list[str]:
    return [
        str(python_executable),
        "-m",
        "west",
        "build",
        "--pristine=auto",
        "-b",
        board,
        "-d",
        str(build_dir),
        str(source_dir),
        "--",
        # Only adds -DNDEBUG (Kconfig sets the optimization level); picolibc used to force it
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
    ]


def run_compile(args, config: ConfigType) -> bool:
    if CORE.using_toolchain_platformio:
        # The actual build is done by PlatformIO (the caller falls through to
        # it when this returns False); prepare the Python environment its
        # Zephyr build script expects first.
        setup_platformio_python_env()
        return False
    if not CORE.using_toolchain_sdk_nrf:
        raise EsphomeError(
            "Unsupported toolchain for nRF52. "
            "Supported toolchains are 'platformio' and 'sdk-nrf'."
        )
    check_and_install()

    paths = get_build_paths()
    # Depend mode in the shared ccache settings keeps the .gch sound
    # across Kconfig flips.
    ccache = resolve_ccache_path()
    env = get_build_env(ccache)

    pch_on = _pch_usable()
    cmake_lists_changed = _generate_cmake_lists(pch_on)

    board = zephyr_data()[KEY_BOARD]
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    source_dir = CORE.relative_build_path("zephyr")

    # A missing CMake cache (dropped by zephyr's copy_files() on config
    # change) or a changed CMakeLists.txt requires a pristine build: Zephyr
    # caches Kconfig/devicetree state that survives a plain cmake re-run.
    # West can't do the wipe — its pristine modes only recognize a build dir
    # by reading ZEPHYR_BASE from the very cache that was dropped.
    if (
        cmake_lists_changed or not (build_dir / "CMakeCache.txt").is_file()
    ) and build_dir.is_dir():
        _LOGGER.info("Build inputs changed, cleaning %s", build_dir)
        rmtree(build_dir)

    # SDK 3.4.0+ no longer generates merged.hex; drop one left by an older SDK
    # build so it is never packaged or offered for download.
    for stale_hex in (build_dir / "merged.hex", build_dir / "zephyr" / "merged.hex"):
        stale_hex.unlink(missing_ok=True)

    if pch_on:
        pch.log_pch_in_use()
        _write_pch_checksum(build_dir, source_dir)

    west_cmd = _west_build_command(
        paths["python_executable"], board, build_dir, source_dir
    )

    if not run_command_ok(
        west_cmd,
        env=env,
        stream_output=True,
        cwd=str(paths["framework_path"]),
    ):
        raise EsphomeError("nRF52 native build failed")

    zephyr_dir = build_dir / "zephyr"
    framework_ver = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    bootloader = zephyr_data()[KEY_BOOTLOADER]

    # (dev_type, sd_req) per bootloader — values from Nordic SoftDevice release notes.
    # The bootloader resets on a package whose sd_req does not list its SoftDevice,
    # so v7 accepts S140 7.0.1, 7.2.0 and 7.3.0 (e.g. Seeed XIAO nRF52840).
    _GENPKG_PARAMS = {
        BOOTLOADER_ADAFRUIT_NRF52_SD132: ("0x0051", "0x009D"),
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: ("0x0052", "0x00B6"),
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: ("0x0052", "0x00CA,0x0100,0x0123"),
    }
    # UF2 family IDs — nRF52832 vs nRF52840 per SoftDevice variant
    _UF2_FAMILY_IDS = {
        BOOTLOADER_ADAFRUIT_NRF52_SD132: "0x7EAED30A",
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V6: "0xADA52840",
        BOOTLOADER_ADAFRUIT_NRF52_SD140_V7: "0xADA52840",
    }

    # SDK < 2.9.2 places artifacts directly in build_dir/zephyr/.
    # SDK >= 2.9.2 nests them one level deeper (build_dir/zephyr/zephyr/);
    # copy files to match get_download_types layout.
    if framework_ver < cv.Version(2, 9, 2):
        west_out = zephyr_dir
    else:
        west_out = zephyr_dir / "zephyr"
        _copy_if_exists(west_out / "zephyr.uf2", zephyr_dir / "zephyr.uf2")
        _copy_if_exists(west_out / "zephyr.signed.bin", zephyr_dir / "app_update.bin")
        _copy_if_exists(west_out / "zephyr.hex", zephyr_dir / "zephyr.hex")
        _copy_if_exists(build_dir / "merged.hex", zephyr_dir / "merged.hex")

    # For Adafruit bootloader builds, regenerate the UF2 from a hex file.
    # merged.hex carries the correct flash addresses; SDK 3.4.0+ no longer
    # generates it, so use zephyr.hex there. Chosen by version so a merged.hex
    # left by an older SDK build is never picked.
    if framework_ver >= cv.Version(3, 4, 0):
        hex_file = zephyr_dir / "zephyr.hex"
    else:
        hex_file = zephyr_dir / "merged.hex"
    if bootloader in _UF2_FAMILY_IDS and hex_file.is_file():
        # Drop the build's own wrong-offset UF2 so it isn't shipped alongside.
        app_uf2 = west_out / "zephyr.uf2"
        if app_uf2.is_file():
            app_uf2.unlink()
        uf2conv = (
            paths["framework_path"] / "zephyr" / "scripts" / "build" / "uf2conv.py"
        )
        if not run_command_ok(
            [
                str(paths["python_executable"]),
                str(uf2conv),
                "-f",
                _UF2_FAMILY_IDS[bootloader],
                "-c",
                "-o",
                str(zephyr_dir / "zephyr.uf2"),
                str(hex_file),
            ],
            env=env,
            stream_output=True,
        ):
            raise EsphomeError(f"Failed to generate UF2 from {hex_file.name}")

    # The serial DFU package follows the hex it is made from; a package from another bootloader's
    # build would otherwise be offered for download with this one
    dfu_package = build_dir / DFU_PACKAGE
    if bootloader not in SERIAL_DFU_BOOTLOADERS:
        dfu_package.unlink(missing_ok=True)
    elif bootloader == BOOTLOADER_NRF:
        # The factory Open DFU bootloader speaks Nordic's Secure DFU protocol, which only Nordic's own
        # tool packages
        if not run_command_ok(
            [
                ensure_nordic_nrfutil(env),
                "nrf5sdk-tools",
                "pkg",
                "generate",
                "--hw-version",
                "52",
                "--sd-req",
                "0x00",
                "--application-version",
                "1",
                "--application",
                str(zephyr_dir / "zephyr.hex"),
                str(dfu_package),
            ],
            env=env,
            stream_output=True,
        ):
            raise EsphomeError("Failed to create the DFU package")
    else:
        genpkg_cmd = [
            str(paths["python_executable"]),
            "-m",
            "nordicsemi.__main__",
            "dfu",
            "genpkg",
        ]
        if bootloader in _GENPKG_PARAMS:
            dev_type, sd_req = _GENPKG_PARAMS[bootloader]
            genpkg_cmd += ["--dev-type", dev_type, "--sd-req", sd_req]
        genpkg_cmd += ["--application", str(hex_file), str(dfu_package)]
        if not run_command_ok(genpkg_cmd, env=env, stream_output=True):
            raise EsphomeError("Failed to create the DFU package")

    from esphome.build_helpers.idedata import warn_if_idedata_missing

    from .toolchain import get_idedata

    warn_if_idedata_missing(get_idedata)
    return True
