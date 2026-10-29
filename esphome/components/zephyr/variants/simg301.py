import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_SIMG301
from ..partitions import BootLayout
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# SixG301 Explorer Kit (BRD2719A). Not in mainline Zephyr yet: set `board_source:` to
# esphomeplatformzephyr/custom_zephyr_boards.
_DEFAULT_BOARD = "simg301_ek2719a"

# EUSART0, the only UART, reaches PA0-PA8 and PB0-PB3 only (six301-pinctrl.h).
_EUSART0_PINS = frozenset([*range(9), *range(16, 20)])

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_SIMG301
VARIANT = ZephyrVariant(
    # Resets from the start of flash (simg301_ek2719a.dts boot_partition@0).
    boot=BootLayout(0x0),
    sdk=MAINLINE,
    sdk_name="zephyr",
    # six301 SoC support first shipped in 4.5.0.
    min_version_override=cv.Version(4, 5, 0),
    family="silabs",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # The SoC has an ieee802154 node too, but it's untested on this chip.
    transports=frozenset({"ble"}),
    blobs=("hal_silabs", ".*", ".blobs_hal_silabs_ready"),
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d"),
    # No scratch partition in the board's flash layout.
    swap_methods=frozenset({"move", "offset"}),
    # The UART node is eusart0, so let DTS discovery find it.
    uart_node_labels={},
    # The Series 3 ADC (silabs,series3-adc) input macros aren't confirmed yet.
    adc_ain_map={},
    uart_valid_pins={"tx": _EUSART0_PINS, "rx": _EUSART0_PINS},
    # EUSART0 SPI pin macros aren't confirmed yet.
    spi_valid_pins={},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "simg301", config, "the six301 SoC support first in 4.5.0"
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
    from .. import (
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_setup_preferences,
    )

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_SIMG301")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "SIMG301")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # The board DTS always links the app into slot0, so it only boots through MCUboot.
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
