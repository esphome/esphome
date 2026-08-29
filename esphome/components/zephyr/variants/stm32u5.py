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

_DEFAULT_BOARD = "stm32u5g9j_dk1"

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
    swap_methods=frozenset({"move", "offset"}),
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d", "e", "f", "g", "h", "i"),

    # wdt_iwdg_stm32.c's IWDG_PRESCALER_MAX is 1024 on U5 (stm32u5xx_ll_iwdg.h supports
    # LL_IWDG_PRESCALER_1024), reload max 4095, LSI 32kHz (dts/arm/st/u5/stm32u5.dtsi):
    # 4095 x 1024 / 32000 =~ 131.04s real ceiling - let's use 60s, satisfying the generic 60s schema max.
    watchdog_max_timeout_ms=60000,
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
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_STM32U5")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "STM32U5")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
    zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
    zephyr_add_prj_conf("HARDWARE_DEVICE_CS_GENERATOR", True)


    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
        zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
