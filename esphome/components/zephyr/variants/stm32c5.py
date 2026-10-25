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
    ZEPHYR_VARIANT_STM32C5,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32c5a3xx), so qualify_board() is a no-op here (soc= is left
# unset below). Largest of ST's three STM32C5 Nucleo boards (1M flash, 256K RAM).
_DEFAULT_BOARD = "nucleo_c5a3zg"

# Like stm32l4, not stm32u5: the stock C5 Nucleo boards declare slot0/slot1 partitions
# but no zephyr,code-partition, so the app links at offset 0 and an MCUboot image would
# not boot. MCUboot is opt-in, with the user's own code-partition overlay.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32C5
VARIANT = ZephyrVariant(
    # Resets from the start of flash (nucleo_c5a3zg.dts boot_partition@0).
    boot=BootLayout(0x0),
    # Seeed's resident TinyUF2 bootloader keeps 0x0-0x8000 and starts the app at
    # 0x8000 (xiao_stm32c5.dts, boot_partition label "uf2-bootloader").
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
    # STM32 GPIO controllers are 16 pins/port, labeled gpioa.. in devicetree. The shared
    # SoC dtsi (dts/arm/st/c5/stm32c5.dtsi) wires up a-e plus h, stm32c591.dtsi adds f
    # and g (C591/C593/C5A3). A port absent from a board's DTS fails at DTS-compile time.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h"),
    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 1024 on C5 (stm32c5xx_ll_iwdg.h has
    # LL_IWDG_PRESCALER_1024), reload max 4095, LSI 32kHz (dts/arm/st/c5/stm32c5.dtsi):
    # 4095 x 1024 / 32000 =~ 131.04s real ceiling - capped at the generic 60s schema max.
    watchdog_max_timeout_ms=60000,
    # Explicitly empty: the C5 Nucleo consoles differ (usart2 on nucleo_c542rc/c562re,
    # lpuart1 on nucleo_c5a3zg). Empty tells resolve_uart_node_label() (dts_lookup.py)
    # to resolve hardware_uart per board.
    uart_node_labels={},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    bootloader = config[CONF_ADVANCED][CONF_BOOTLOADER]
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
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
    # No STM32C5 board defconfig (nucleo_c5*, xiao_stm32c5) sets CONFIG_GPIO=y,
    # unlike the older STM32 boards; without it gpio devices are never built.
    zephyr_add_prj_conf("GPIO", True)
    # get_mac_address_raw() (zephyr/core.cpp) derives the MAC from the unique ID via hwinfo.
    zephyr_add_prj_conf("HWINFO", True)

    # dts/arm/st/c5 declares no rng@ node, so this falls back to TEST_RANDOM_GENERATOR
    # today; resolved from the board's DTS so a board or SoC dtsi that adds one is used.
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
