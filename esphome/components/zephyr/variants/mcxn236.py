import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    CONF_WATCHDOG_TIMEOUT,
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
    ZEPHYR_VARIANT_MCXN236,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_DEFAULT_BOARD = "frdm_mcxn236"

# frdm_mcxn236.dts sets zephyr,code-partition = &slot0_partition and the chip resets
# to 0x0, so only MCUboot boots it.
_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_MCUBOOT): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_MCXN236
VARIANT = ZephyrVariant(
    # Resets from the start of flash (frdm_mcxn236.dts boot_partition@0).
    boot=BootLayout(0x0),
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="nxp",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    transports=frozenset(),
    # MCUboot's frdm_mcxn236.conf is upgrade-only; NXP says swap modes don't work here.
    swap_methods=frozenset(),
    gpio_port_width=32,
    # The WWDT counts 1 MHz / 4, about 67 s at most.
    watchdog_max_timeout_ms=60000,
    # Empty: the console UART is found per board (flexcomm4_lpuart4 on frdm_mcxn236).
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
        VARIANT, "mcxn236", config, "MCX N support"
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

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_MCXN236")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "MCXN236")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    # NVS can't mount on this flash's 128-byte write blocks; settings picks ZMS when it is enabled.
    zephyr_add_prj_conf("ZMS", True)
    zephyr_add_prj_conf("ZMS_CUSTOMIZE_BLOCK_SIZE", True)
    zephyr_add_prj_conf("ZMS_CUSTOM_BLOCK_SIZE", 128)
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) derives the MAC from the unique ID via hwinfo.
    zephyr_add_prj_conf("HWINFO", True)
    if config[CONF_WATCHDOG_TIMEOUT].total_milliseconds:
        # Without a warning time the driver rejects hal.cpp's callback plus reset.
        zephyr_add_prj_conf("WDT_MCUX_WWDT_WARNING_INTERRUPT_CFG", 1023)

    # Older SoC dtsi files have no TRNG node.
    rng_label = get_rng_node_label(config[CONF_BOARD], "nxp,els-trng")
    if rng_label is not None:
        zephyr_add_prj_conf("ENTROPY_GENERATOR", True)
        zephyr_add_overlay(f'&{rng_label} {{ status = "okay"; }};')
    else:
        zephyr_add_prj_conf("TEST_RANDOM_GENERATOR", True)

    if zephyr_data()[KEY_BOOTLOADER] == BOOTLOADER_MCUBOOT:
        zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
        # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
        zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
