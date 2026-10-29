import os
from pathlib import Path
import time

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.core import CORE, CoroPriority, EsphomeError, coroutine_with_priority
from esphome.types import ConfigType

from ..board_revision import parse_board_string
from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_SCHEMA,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOARD,
    KEY_BOOTLOADER,
    ZEPHYR_VARIANT_STM32C5,
    zephyr_ns,
)
from ..partitions import BootLayout
from ..usb_baud_rate import CONF_USB_BAUD_RATE, UsbBaudRate, declare_component_id
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# The largest C5 Nucleo (1M flash, 256K RAM).
_DEFAULT_BOARD = "nucleo_c5a3zg"

# MCUboot is opt-in: the C5 Nucleo boards have slot partitions but no zephyr,code-partition.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32C5
VARIANT = ZephyrVariant(
    boot=BootLayout(0x0),
    # Seeed's TinyUF2 bootloader keeps 0x0-0x8000.
    board_boot={
        "xiao_stm32c5": BootLayout(0x8000, ((0x0, 0x8000),)),
    },
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # The stock C5 Nucleo layouts have no scratch_partition.
    swap_methods=frozenset({"move", "offset"}),
    # stm32c5.dtsi has ports a-e and h; stm32c591.dtsi adds f and g.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h"),
    # The IWDG reaches about 131 s on C5; capped at the schema's 60 s.
    watchdog_max_timeout_ms=60000,
    # Empty: the C5 Nucleo consoles differ (usart2, lpuart1), so they are found per board.
    uart_node_labels={},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _xiao_config_schema(config)
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "stm32", config, "mainline STM32 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        bootloader if bootloader == BOOTLOADER_MCUBOOT else "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_overlay,
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_data,
        zephyr_setup_preferences,
    )
    from ..dts_lookup import get_rng_node_label

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32C5")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32C5")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) derives the MAC from the unique ID via hwinfo.
    zephyr_add_prj_conf("HWINFO", True)
    _xiao_to_code(config)

    # The C5 SoC dtsi has no RNG node; a board that adds one gets the hardware RNG.
    rng_label = get_rng_node_label(config[CONF_BOARD])
    if rng_label is not None:
        zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
        zephyr_add_overlay(f'&{rng_label} {{ status = "okay"; }};')
    else:
        zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)


# Seeed XIAO STM32C5; values read from its TinyUF2 bootloader and factory app.
_XIAO_BOARD = "xiao_stm32c5"
# Seeed's own; Zephyr's uf2families.json has no STM32C5.
_XIAO_UF2_FAMILY_ID = 0x00C5C5C5
# TinyUF2's double-tap word, the last word of the 256K SRAM.
_XIAO_DBL_TAP_ADDR = 0x2003FFFC
# The bootloader's USB vendor and product IDs.
_XIAO_UF2_USB_ID = ("2886", "00c5")
_UF2_DISK_TIMEOUT = 15.0

TinyUf2Touch = zephyr_ns.class_("TinyUf2Touch", cg.Component)
CONF_TINYUF2_TOUCH = "tinyuf2_touch"

# Internal keys, filled in by _xiao_config_schema().
_ADVANCED_SCHEMA = _ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_TINYUF2_TOUCH): declare_component_id(TinyUf2Touch),
        cv.Optional(CONF_USB_BAUD_RATE): declare_component_id(UsbBaudRate),
    }
)


def _is_xiao(board: str) -> bool:
    return parse_board_string(board).name == _XIAO_BOARD


def _xiao_config_schema(config: ConfigType) -> None:
    # Components are counted at validation, so they cannot be created later in to_code.
    if _is_xiao(config[CONF_BOARD]):
        advanced = config[CONF_ADVANCED]
        if advanced[CONF_BOOTLOADER] == BOOTLOADER_MCUBOOT:
            raise cv.Invalid(
                "The XIAO STM32C5's TinyUF2 bootloader cannot be replaced by MCUboot",
                [CONF_ADVANCED, CONF_BOOTLOADER],
            )
        advanced.setdefault(
            CONF_TINYUF2_TOUCH, declare_component_id(TinyUf2Touch)(None)
        )
        advanced.setdefault(CONF_USB_BAUD_RATE, declare_component_id(UsbBaudRate)(None))


def _xiao_to_code(config: ConfigType) -> None:
    from .. import zephyr_add_overlay, zephyr_add_prj_conf
    from ..dts_lookup import dts_node_enabled, has_pinctrl_configured

    if _is_xiao(config[CONF_BOARD]):
        zephyr_add_prj_conf("BUILD_OUTPUT_UF2", True)
        zephyr_add_prj_conf(
            "BUILD_OUTPUT_UF2_FAMILY_ID", f"{_XIAO_UF2_FAMILY_ID:#010x}"
        )
        cg.add_define("USE_ZEPHYR_TINYUF2_TOUCH")
        CORE.add_job(_tinyuf2_touch_to_code, config[CONF_ADVANCED])
        # The board DTS has no USB clock or pins; each is added only while it is missing.
        if not dts_node_enabled(config[CONF_BOARD], "clk48"):
            # HSE is 48 MHz on this board.
            zephyr_add_overlay(
                '&clk48 { clocks = <&rcc STM32_SRC_HSE CK48_SEL(3)>; status = "okay"; };'
            )
        if not has_pinctrl_configured(config[CONF_BOARD], "usb"):
            zephyr_add_overlay(
                '&usb { pinctrl-0 = <&usb_dm_pa11 &usb_dp_pa12>; pinctrl-names = "default"; };'
            )


@coroutine_with_priority(CoroPriority.DIAGNOSTICS)
async def _tinyuf2_touch_to_code(advanced: ConfigType) -> None:
    from .. import zephyr_add_usb_baud_rate

    usb_baud_rate = await zephyr_add_usb_baud_rate(advanced)
    var = cg.new_Pvariable(
        advanced[CONF_TINYUF2_TOUCH],
        usb_baud_rate,
        cg.RawExpression(hex(_XIAO_DBL_TAP_ADDR)),
    )
    await cg.register_component(var, {})


def uses_tinyuf2() -> bool:
    from .. import zephyr_data

    return _is_xiao(zephyr_data()[KEY_BOARD])


def _usb_id(disk: Path) -> tuple[str, str] | None:
    """USB vendor and product IDs of the device behind a disk, from sysfs."""
    device = Path("/sys/class/block", disk.resolve().name, "device").resolve()
    for parent in device.parents:
        if (parent / "idVendor").is_file():
            vendor = (parent / "idVendor").read_text().strip()
            return vendor, (parent / "idProduct").read_text().strip()
    return None


def _find_uf2_disk() -> Path | None:
    # Other TinyUF2 boards use the same disk name.
    disks = Path("/dev/disk/by-id").glob("usb-Adafruit_UF2_Bootloader_*")
    return next((d for d in disks if _usb_id(d) == _XIAO_UF2_USB_ID), None)


def upload_program(host: str) -> bool:
    """UF2 bootloaders accept blocks written anywhere on the disk, so it is not mounted."""
    import serial

    uf2 = CORE.relative_build_path(".west_build/zephyr/zephyr/zephyr.uf2")
    if (disk := _find_uf2_disk()) is None:
        try:
            serial.Serial(host, baudrate=1200).close()
        except serial.SerialException as err:
            raise EsphomeError(f"Could not open {host}: {err}") from err
        deadline = time.monotonic() + _UF2_DISK_TIMEOUT
        while (disk := _find_uf2_disk()) is None:
            if time.monotonic() > deadline:
                raise EsphomeError(
                    "The TinyUF2 bootloader did not appear. Put the device into "
                    "bootloader mode and run the upload again."
                )
            time.sleep(0.5)
    try:
        with disk.open("wb") as f:
            f.write(uf2.read_bytes())
            # Another process (udev) may hold the disk open, so closing it may not flush.
            os.fsync(f.fileno())
    except PermissionError as err:
        raise EsphomeError(
            f"No write access to {disk}. Add a udev rule that gives your user write "
            'access, for example: SUBSYSTEM=="block", ATTRS{idVendor}=="2886", '
            'ATTRS{idProduct}=="00c5", GROUP="plugdev", MODE="0660"'
        ) from err
    return True
