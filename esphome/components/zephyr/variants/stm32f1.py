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
    ZEPHYR_VARIANT_STM32F1,
)
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32f103xb), same as stm32f4.py/stm32l4.py. ST's own Nucleo-64
# eval board for the STM32F1 line, the smallest MCU family ESPHome's Zephyr platform
# targets: 128K flash/20K RAM total, versus 512K/96K on stm32f4's nucleo_f401re.
_DEFAULT_BOARD = "nucleo_f103rb"

# Like stm32f4/wb55, not stm32l4: nucleo_f103rb's stock flash0 layout is already
# MCUboot-shaped (34K boot_partition, 45K/46K slot0/slot1, 3K storage, no
# scratch_partition) with `zephyr,code-partition = &slot0_partition` set in the board's
# own DTS -- same as nucleo_f401re. Unlike ra4m1/rp2040 (which boot from a real resident
# first-stage bootloader at offset 0), this Nucleo has no bootloader of its own: the CPU
# always resets to address 0, so a `none`-built image linked for slot0_partition
# (offset 0x8800) would never run. mcuboot has to stay the default here; `none` remains
# selectable only for a different F1 board whose own DTS boots from offset 0.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32F1
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # nucleo_f103rb's stock layout has no scratch_partition (unlike stm32f4's
    # nucleo_f401re) -- only "move"/"offset" are supported out of the box.
    swap_methods=frozenset({"move", "offset"}),
    # STM32 GPIO controllers are 16 pins/port (dts/bindings/gpio/st,stm32-gpio.yaml's
    # ngpios defaults to 16), labeled gpioa..gpiog in devicetree -- same scheme as
    # stm32f4/stm32l4/stm32wb55. The full a-g range covers every STM32F1 board (dts/arm/
    # st/f1/*.dtsi wires gpioa through gpiog across the family's various dies), even
    # though the default board (nucleo_f103rb) only wires up a-e in its own SoC dtsi;
    # requesting a port absent from a given board's DTS fails at DTS-compile time with a
    # clear error, the same class of failure an out-of-range pin already produces.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g"),
    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 256 on F1 (stm32f1xx_ll_iwdg.h has no
    # LL_IWDG_PRESCALER_1024, same as F4/L4/WB55), reload max 4095, but LSI is 40kHz on
    # F1 (dts/arm/st/f1/stm32f1.dtsi's clk-lsi) rather than the 32kHz used by those other
    # families: 4095 x 256 / 40000 =~ 26.21s real ceiling, below the generic 60s schema
    # max.
    watchdog_max_timeout_ms=26000,
    # Explicitly empty, not the {"UART0": "uart0", "UART1": "uart1"} default: STM32
    # boards don't share a portable UART naming or console convention (nucleo_f103rb's
    # console is usart2, other STM32F1 boards may use a different instance). Empty tells
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
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_data,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32F1")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32F1")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)

    # No STM32F1 die has a true RNG peripheral (dts/arm/st/f1 has no rng@ node anywhere
    # in the family, unlike F4 where larger members do) -- unconditional, no per-board
    # detection needed.
    zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
