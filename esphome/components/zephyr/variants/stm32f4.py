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

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    BOOTLOADER_NONE,
    CONF_BOOTLOADER,
    CONF_RUNNER,
    KEY_BOOTLOADER,
    ZEPHYR_VARIANT_STM32F4,
)
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32f401xe), so qualify_board() is a no-op here (soc= is left
# unset below). ST's own Nucleo-64 eval board for the STM32F4 line, same choice of
# board tier as stm32l4.py's nucleo_l476rg. Its stock flash0 partition table is
# MCUboot-shaped (boot/slot0/slot1/scratch) but has no storage_partition -- every
# config on this variant is expected to add one via `overlays:`.
_DEFAULT_BOARD = "nucleo_f401re"

# Unlike stm32l4/rp2040/ra4m1, bootloader: defaults to mcuboot here -- real MCUboot
# (ECDSA-P256) fits the stock boot_partition with room to spare (measured 24-31K of
# 32K), hardware-verified on nucleo_f401re/blackpill_f411ce. none stays selectable
# for boards with their own resident bootloader (e.g. WeAct's HID bootloader).
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32F4
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # nucleo_f401re's stock layout ships its own scratch_partition, so "scratch" is
    # included (unlike stm32l4, whose board has no MCUboot partitions at all).
    swap_methods=frozenset({"scratch", "move", "offset"}),
    # STM32 GPIO controllers are 16 pins/port (dts/bindings/gpio/st,stm32-gpio.yaml's
    # ngpios defaults to 16), labeled gpioa..gpioh in devicetree -- same scheme as
    # stm32l4.py. The F4 family's shared SoC dtsi (dts/arm/st/f4/stm32f4.dtsi) wires up
    # the full a-h range; requesting a port absent from a given board's DTS fails at
    # DTS-compile time with a clear error, the same class of failure an out-of-range pin
    # already produces.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h"),
    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 256 on F4 (stm32f4xx_ll_iwdg.h has no
    # LL_IWDG_PRESCALER_1024, same as L4), reload max 4095, LSI 32kHz (dts/arm/st/f4/
    # stm32f4.dtsi): 4095 x 256 / 32000 =~ 32.76s real ceiling, below the generic 60s
    # schema max.
    watchdog_max_timeout_ms=32000,
    # Explicitly empty, not the {"UART0": "uart0", "UART1": "uart1"} default: STM32
    # boards don't share a portable UART naming or console convention (nucleo_f401re's
    # console is usart2, other STM32F4 boards use different instances). Empty tells
    # resolve_uart_node_label() (dts_lookup.py) to resolve hardware_uart per board.
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
        zephyr_to_code,
    )
    from ..dts_lookup import get_rng_node_label

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32F4")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32F4")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)

    # RNG presence varies per STM32F4 member (F401/F411 have none, F405/F410/F412 and
    # larger do) -- resolved from the board's own DTS rather than assumed. The node
    # ships `status = "disabled"` in the SoC dtsi itself with no board turning it on,
    # so it has to be enabled explicitly once found.
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
