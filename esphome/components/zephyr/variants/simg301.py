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

# SixG301 Explorer Kit (BRD2719A). Out-of-tree board (esphome-zephyr's own boards/, see
# zephyr: board_source:) -- not yet upstream, tracked at
# https://github.com/zephyrproject-rtos/zephyr/issues/121047. Bare name; no SoC/qualifier
# segment needed, same reasoning as efr32mg24's xg24_ek2703a.
_DEFAULT_BOARD = "simg301_ek2719a"

# EUSART0 is this SoC's only UART-capable peripheral, and its DBUS crossbar only reaches
# ports A (9 pins) and B (4 pins) -- confirmed against
# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/silabs/six301-pinctrl.h,
# unlike EFR32 Series 2's full 4-port crossbar (efr32mg24.py's _GPIO_MATRIX_PINS). Packed
# the same way: port index = p // 16, pin number = p % 16.
_EUSART0_PINS = frozenset([*range(9), *range(16, 20)])

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_SIMG301
VARIANT = ZephyrVariant(
    # Resets from the start of flash (simg301_ek2719a.dts boot_partition@0).
    boot=BootLayout(0x0),
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="silabs",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # BLE confirmed available (bt_hci_silabs HCI node enabled upstream, same radio IP as
    # EFR32 Series 2). The ieee802154 node exists on this SoC too but is unverified on
    # real Series-3 hardware -- not claimed here.
    transports=frozenset({"ble"}),
    blobs=("hal_silabs", ".*", ".blobs_hal_silabs_ready"),
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d"),
    # No scratch partition in the board's flash layout (boot/image-0/image-1/storage
    # only), same shape as efr32mg24 -- move/offset need no scratch, "swap" would.
    swap_methods=frozenset({"move", "offset"}),
    # Only eusart0 exists at SoC level -- not uart0/uart1, so dynamic DTS discovery
    # must resolve UART0's real label instead of the generic uart0/uart1 default.
    uart_node_labels={},
    # Series-3 ADC is a different compatible ("silabs,series3-adc") than EFR32 Series 2's
    # IADC -- not confirmed to use the same IADC_INPUT_P<port><pin> macro naming, so left
    # unset rather than guessed.
    adc_ain_map={},
    uart_valid_pins={"tx": _EUSART0_PINS, "rx": _EUSART0_PINS},
    # EUSART0 can run as a synchronous SPI master too (Simplicity SDK ships a
    # spidrv_eusart "mikroe" config for this board), but the exact clk/mosi/miso pin
    # macros aren't confirmed yet -- left empty ("not wired up") rather than guessed.
    spi_valid_pins={},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    if not config[CONF_FRAMEWORK].get(CONF_SOURCE):
        raise cv.Invalid(
            "SIMG301 needs Zephyr's silabs_s3/six301 SoC support, which merged after "
            "the v4.4.2 tag and isn't in any tagged release yet. Set `framework: "
            "source: {ref: main}` (or pin a specific commit) until a release ships it.",
            path=[CONF_FRAMEWORK],
        )
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "simg301", config, "SiMG301 support"
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

    # simg301_ek2719a.dts hardcodes the boot/image-0/image-1/storage partition layout and
    # `zephyr,code-partition = &slot0_partition` unconditionally, same as efr32mg24's
    # xg24_ek2703a -- the app can only ever boot via MCUboot jumping to slot0.
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
