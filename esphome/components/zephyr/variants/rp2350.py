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
    ZEPHYR_VARIANT_RP2350,
)
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
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# GPIO -> RP2350 ADC channel index. Same fixed-function silicon mapping as RP2040
# (see variants/rp2040.py): only GPIO26-29 route to the ADC, channel = pin - 26.
# Confirmed against Zephyr's raspberrypi,pico-adc binding and
# rpi-pico-rp2350a-pinctrl.h's ADC_CH0_P26..ADC_CH3_P29 macros (identical names to
# RP2040's header).
_ADC_CHANNEL_MAP = {26: 0, 27: 1, 28: 2, 29: 3}

VARIANT_NAME = ZEPHYR_VARIANT_RP2350
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
    soc="rp2350a",
    qualifier="m33",
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=30,
    adc1_channel_map=_ADC_CHANNEL_MAP,
    uart_node_labels={},
    # Same base mux as RP2040, plus a second alternate UART function on a disjoint
    # pin set (GPIO_FUNC_UART_ALT) -- verified against the real pinctrl dt-bindings.
    uart_valid_pins_by_instance={
        "UART0": {
            "tx": frozenset({0, 2, 12, 14, 16, 18, 28}),
            "rx": frozenset({1, 3, 13, 15, 17, 19, 29}),
        },
        "UART1": {
            "tx": frozenset({4, 6, 8, 10, 20, 22, 24, 26}),
            "rx": frozenset({5, 7, 9, 11, 21, 23, 25, 27}),
        },
    },
    # A single "pwm" controller node covers all 8 slices reachable within the 30
    # GPIOs this variant exposes (P0-P29) -- repeat the label once per slice so
    # zephyr_pwm's block-count math (len(pwm_node_labels)) still works unmodified.
    pwm_node_labels=["pwm"] * 8,
    pwm_channels_per_block=2,
    # wdt_rpi_pico.c: RPI_PICO_MAX_WDT_TIME = 0xFFFFFF us (no errata halving on
    # RP2350) -- real ceiling ~16.78s, above the 10s default but below the generic
    # 60s schema max.
    watchdog_max_timeout_ms=16000,
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
    _, framework_ver, sdk_name, _ = resolve_framework_version(
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
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
