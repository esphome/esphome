import logging

import esphome.codegen as cg
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_SCHEMA,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOOTLOADER,
    ZEPHYR_VARIANT_RP2040,
)
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

# bootloader: mcuboot only controls the sysbuild/signing confs below -- it never rewrites
# board:. Auto-appending /mcuboot onto a bare board name would target a west board that
# may not exist (only rpi_pico/rpi_pico_rp2040_w have an upstream vendor .../mcuboot DTS;
# a custom board has no such sibling at all). Anyone choosing mcuboot must supply the
# fully qualified board themselves, e.g. board: rpi_pico/rp2040/mcuboot, same as they'd
# supply their own MCUboot-shaped partitions via overlays: for a board without one.

_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# GPIO -> RP2040 ADC channel index. Fixed-function silicon: only GPIO26-29 route to
# the ADC, channel = pin - 26 (Espressif-shaped: the devicetree channel@N address IS
# the real silicon channel). Confirmed against Zephyr's raspberrypi,pico-adc binding
# and rpi-pico-rp2040-pinctrl.h's ADC_CH0_P26..ADC_CH3_P29 macros.
_ADC_CHANNEL_MAP = {26: 0, 27: 1, 28: 2, 29: 3}

VARIANT_NAME = ZEPHYR_VARIANT_RP2040
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="rpi_pico",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset({"wifi"}),
    transport_drivers={
        "wifi": ("CYW43439", "{/soc/pio@50200000/pio0_spi0/airoc-wifi@0}")
    },
    transport_blobs={
        "wifi": ("hal_infineon", ".*43439.*", ".blobs_hal_infineon_ready")
    },
    soc="rp2040",
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=30,
    adc1_channel_map=_ADC_CHANNEL_MAP,
    # A single "pwm" controller node covers all 8 slices reachable within the 30
    # GPIOs this variant exposes (P0-P29) -- repeat the label once per slice so
    # zephyr_pwm's block-count math (len(pwm_node_labels)) still works unmodified.
    pwm_node_labels=["pwm"] * 8,
    pwm_channels_per_block=2,
    # wdt_rpi_pico.c: RPI_PICO_MAX_WDT_TIME = 0xFFFFFF us, halved on RP2040 by errata
    # RP2040-E1 -- real ceiling ~8.39s, below the generic 10s default.
    watchdog_max_timeout_ms=8000,
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
    _, framework_ver, sdk_name, _ = resolve_framework_version(
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
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
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
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
