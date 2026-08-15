import esphome.codegen as cg
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_STM32L4
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board (stm32l476xx), so qualify_board() is a no-op here (soc= is left
# unset below).
_DEFAULT_BOARD = "nucleo_l476rg"

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_STM32L4
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="stm32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    swap_methods=frozenset({"move", "offset"}),
    # STM32 GPIO controllers are 16 pins/port (dts/bindings/gpio/st,stm32-gpio.yaml's
    # ngpios defaults to 16), labeled gpioa..gpioh in devicetree -- not Nordic/Espressif's
    # flat gpio0/gpio1 numeric scheme. The full a-h range covers every STM32L4 board, even
    # though the default board (nucleo_l476rg) only wires up a/b/c/h in its SoC dtsi;
    # requesting a port absent from a given board's DTS fails at DTS-compile time with a
    # clear error, the same class of failure an out-of-range pin already produces.
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h"),
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "stm32", config, "mainline STM32 support"
    )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        BOOTLOADER_MCUBOOT,
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
    )
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32L4")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32L4")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
