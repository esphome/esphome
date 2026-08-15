import logging

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
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER, ZEPHYR_VARIANT_RP2040
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

_DEFAULT_BOARD = "rpi_pico"

# RP2040's ROM has its own USB (BOOTSEL) bootloader, so unlike every other mainline
# variant, a software bootloader here is optional rather than required to flash at all.
# Default to none -- MCUboot only buys OTA/rollback, at the cost of a second board target
# (rpi_pico/rp2040/mcuboot) upstream ships as a separate DTS, not a config layered onto
# the default board the way BOOTLOADER_MCUBOOT sysbuild conf works for other chips.
CONF_BOOTLOADER = "bootloader"
BOOTLOADER_NONE = "none"

# bootloader: mcuboot only controls the sysbuild/signing confs below -- it never rewrites
# board:. Auto-appending /mcuboot onto a bare board name would target a west board that
# may not exist (only rpi_pico/rpi_pico_rp2040_w have an upstream vendor .../mcuboot DTS;
# a custom board has no such sibling at all). Anyone choosing mcuboot must supply the
# fully qualified board themselves, e.g. board: rpi_pico/rp2040/mcuboot, same as they'd
# supply their own MCUboot-shaped partitions via overlays: for a board without one.

_ADVANCED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_NONE): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

VARIANT_NAME = ZEPHYR_VARIANT_RP2040
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="rpi_pico",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    soc="rp2040",
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=30,
    pwm_node_labels=["pwm"],
)


def config_schema(config: ConfigType) -> ConfigType:
    # Cannot use kconfig_option: CONFIG_SMP -- mainline Zephyr has no SMP
    # support for RP2040.
    _LOGGER.warning(
        "RP2040 is dual-core, but Zephyr has no SMP support for this SoC yet -- only one core "
        "is used."
    )
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "rp2040", config, "RP2040 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        bootloader if bootloader == BOOTLOADER_MCUBOOT else "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_data,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_RP2040")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2040")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)
    zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
