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

from ..const import BOOTLOADER_MCUBOOT, KEY_BOOTLOADER, ZEPHYR_VARIANT_RP2350
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

_DEFAULT_BOARD = "rpi_pico2"

# Same reasoning as RP2040 (see variants/rp2040.py): the RP2350 ROM's own USB (BOOTSEL)
# bootloader can flash the chip without any software bootloader, so MCUboot is opt-in
# rather than required. Only some boards (e.g. rpi_pico2) ship an upstream vendor
# .../mcuboot DTS sibling -- xiao_rp2350 does not -- so anyone choosing mcuboot must
# supply the fully qualified board themselves, e.g. board: rpi_pico2/rp2350a/m33/mcuboot.
CONF_BOOTLOADER = "bootloader"
BOOTLOADER_NONE = "none"

_ADVANCED_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_NONE): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

VARIANT_NAME = ZEPHYR_VARIANT_RP2350
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="rpi_pico",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    soc="rp2350a",
    qualifier="m33",
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=30,
    pwm_node_labels=["pwm"],
)


def config_schema(config: ConfigType) -> ConfigType:
    # Cannot use kconfig_option: CONFIG_SMP -- mainline Zephyr has no SMP
    # support for RP2350 yet either (same as RP2040).
    _LOGGER.warning(
        "RP2350 is dual-core, but Zephyr has no SMP support for this SoC yet -- only one "
        "core is used."
    )
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "rp2350", config, "RP2350 support"
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_RP2350")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "RP2350")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)
    # RP2350's hardware TRNG driver hangs the boot sequence (unbounded busy-wait) --
    # use the same software-PRNG fallback as RP2040 until fixed upstream.
    zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
