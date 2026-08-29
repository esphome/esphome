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
    ZEPHYR_VARIANT_STM32WB55,
)
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32wb55xx), same as stm32f4.py/stm32l4.py. ST's own Nucleo-64
# eval board for the STM32WB55 line -- same chip (stm32wb55xx), same 876K flash/192K RAM
# as WeAct's stm32wb55_core, the board this variant targets. Its stock flash0 partition
# table is already MCUboot-shaped (boot/slot0/slot1/storage) but has no
# scratch_partition, unlike WeAct's own board which reserves 16K for one.
_DEFAULT_BOARD = "nucleo_wb55rg"

# Like stm32f4, not stm32l4: the default board's stock layout is already MCUboot-shaped
# (boot_partition 48K, slot0/slot1 ~400K each) with plenty of room for real MCUboot
# (ECDSA-P256), so bootloader: defaults to mcuboot here. none stays selectable for a
# board with its own resident bootloader.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32WB55
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # nucleo_wb55rg's stock layout has no scratch_partition (unlike stm32f4's
    # nucleo_f401re) -- only "move"/"offset" are supported out of the box.
    swap_methods=frozenset({"move", "offset"}),
    # STM32 GPIO controllers are 16 pins/port (dts/bindings/gpio/st,stm32-gpio.yaml's
    # ngpios defaults to 16), labeled gpioa..gpioh in devicetree -- same scheme as
    # stm32f4/stm32l4. Unlike those families, STM32WB55's own SoC dtsi
    # (dts/arm/st/wb/stm32wb.dtsi) only wires up gpioa-gpioe and gpioh -- no gpiof/gpiog
    # nodes exist at all for this chip, so they're omitted here rather than left in to
    # fail at DTS-compile time.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "h"),
    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 256 on WB (stm32wbxx_ll_iwdg.h has no
    # LL_IWDG_PRESCALER_1024, same as F4/L4), reload max 4095, LSI 32kHz (dts/arm/st/wb/
    # stm32wb.dtsi): 4095 x 256 / 32000 =~ 32.76s real ceiling, below the generic 60s
    # schema max.
    watchdog_max_timeout_ms=32000,
    # Explicitly empty, not the {"UART0": "uart0", "UART1": "uart1"} default: STM32
    # boards don't share a portable UART naming or console convention (nucleo_wb55rg's
    # console is usart1, other STM32WB55 boards may use a different instance). Empty
    # tells resolve_uart_node_label() (dts_lookup.py) to resolve hardware_uart per board.
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32WB55")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32WB55")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)

    # STM32WB55 has a true RNG (dts/arm/st/wb/stm32wb.dtsi's rng@58001000) needed for its
    # BLE security stack -- present on every WB55 board, unlike F401/F411 which have none.
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
