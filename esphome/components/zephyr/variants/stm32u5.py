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
    ZEPHYR_VARIANT_STM32U5,
)
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32u5g9xx), so qualify_board() is a no-op here (soc= is left
# unset below). ST's own Discovery kit for the STM32U5 line. Its stock flash0 partition
# table is already MCUboot-shaped (boot 64K / slot0 1952K / slot1 1960K / storage 120K),
# so unlike stm32f4's nucleo_f401re no `overlays:` storage_partition is needed.
_DEFAULT_BOARD = "stm32u5g9j_dk1"

# Like stm32l4/f1, not stm32f4/wb55: bootloader: defaults to none here and is opted into
# with `bootloader: mcuboot`.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(BOOTLOADER_SCHEMA)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32U5
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # stm32u5g9j_dk1's stock layout has no scratch_partition (like stm32wb55's
    # nucleo_wb55rg, unlike stm32f4's nucleo_f401re) -- only "move"/"offset" work
    # out of the box.
    swap_methods=frozenset({"move", "offset"}),
    # STM32 GPIO controllers are 16 pins/port (dts/bindings/gpio/st,stm32-gpio.yaml's
    # ngpios defaults to 16), labeled gpioa.. in devicetree -- same scheme as
    # stm32f4/stm32l4/stm32wb55. Which ports exist varies by U5 sub-family: the shared
    # SoC dtsi (dts/arm/st/u5/stm32u5.dtsi) wires up a-e plus g/h, stm32u5_extra.dtsi
    # adds f and i (reached only via the usbotg_fs/usbotg_hs dtsi, i.e. U575/U595 and
    # up), and stm32u595.dtsi adds j. The full a-j union is listed here rather than the
    # a-e/g-h intersection: requesting a port absent from a given board's DTS fails at
    # DTS-compile time with a clear error, the same class of failure an out-of-range pin
    # already produces.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h", "i", "j"),
    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 1024 on U5 (stm32u5xx_ll_iwdg.h supports
    # LL_IWDG_PRESCALER_1024), reload max 4095, LSI 32kHz (dts/arm/st/u5/stm32u5.dtsi):
    # 4095 x 1024 / 32000 =~ 131.04s real ceiling - let's use 60s, satisfying the generic 60s schema max.
    watchdog_max_timeout_ms=60000,
    # Explicitly empty, not the {"UART0": "uart0", "UART1": "uart1"} default: STM32
    # boards don't share a portable UART naming or console convention (stm32u5g9j_dk1's
    # console is usart1, other STM32U5 boards use different instances). Empty tells
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32U5")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32U5")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)

    # Every STM32U5 member has a true RNG, but the node ships `status = "disabled"` in
    # the SoC dtsi itself (dts/arm/st/u5/stm32u5.dtsi's rng@420c0800), so it has to be
    # enabled explicitly -- stm32u5g9j_dk1 happens to enable it in its own DTS, a custom
    # board may not. Resolved from the board's own DTS rather than assumed, same as
    # stm32f4/stm32wb55: without this the CSPRNG choice silently falls back to the
    # insecure TEST_CSPRNG_GENERATOR instead of failing loudly.
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
