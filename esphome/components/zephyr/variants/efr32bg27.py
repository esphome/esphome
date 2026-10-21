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
    CONF_RUNNER,
    ZEPHYR_VARIANT_EFR32BG27,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    SILABS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# xG27-DK2602A (BRD2602A), Silicon Labs' EFR32xG27 Dev Kit. Bare name; no
# SoC/qualifier segment needed -- upstream's board.yml declares a single SoC
# (efr32bg27c140f768im40) for this board, so qualify_board() is a no-op here
# (soc= is left unset below).
_DEFAULT_BOARD = "xg27_dk2602a"

# framework: type: silabs only -- overrides commander_setup.py's pinned default. Its own
# release cadence is independent of the silabs SDK version, same reasoning as
# west_version:/ninja_version: vs. the Zephyr version.
CONF_COMMANDER_VERSION = "commander_version"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_COMMANDER_VERSION): cv.string_strict,
    }
)

# GPIO -> Silicon Labs IADC analog-input macro. Built from this SoC's own bonded
# pin counts (ngpios: A=9, B=5, C=8, D=4, confirmed against its dtsi) rather than
# reused from EFR32MG24, which bonds out the full PA0-PD15 range.
_BONDED_PINS_BY_PORT = {0: 9, 1: 5, 2: 8, 3: 4}  # port index -> ngpios
_ADC_AIN_MAP = {
    port * 16 + pin: f"IADC_INPUT_P{chr(ord('A') + port)}{pin}"
    for port, count in _BONDED_PINS_BY_PORT.items()
    for pin in range(count)
}

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/silabs/xg27-pinctrl.h
# Full crossbar within the bonded pin set above -- every USART/EUSART TX/RX/CLK signal
# has a macro for each of PA0-8/PB0-4/PC0-7/PD0-3, no exclusions (unlike esp32). Shared
# by every free-mux GPIO signal (UART, SPI).
_GPIO_MATRIX_PINS = frozenset(_ADC_AIN_MAP)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_EFR32BG27
VARIANT = ZephyrVariant(
    # Resets from the start of flash (xg27_dk2602a.dts boot_partition@0).
    boot=BootLayout(0x0),
    # Mainline stays default; Silicon Labs' vendor SDK (SILABS) is available as an alt
    # (framework: type: silabs) pending real hardware testing.
    sdk=MAINLINE,
    sdk_name="zephyr",
    alt_sdks={"silabs": SILABS},
    family="silabs",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # BLE only -- EFR32BG27 is a Blue Gecko part, its radio node (efr32xg27.dtsi) only
    # declares ble-2mbps/ble-coded-phy/ble-cte-tx properties and has no ieee802154 node
    # at all, unlike EFR32MG24. No OpenThread/Zigbee possible on this chip.
    transports=frozenset({"ble"}),
    # Zephyr's hal_silabs blob check verifies the HAL's entire manifest once BT is
    # enabled, not just this chip's -- same reasoning as efr32mg24.
    blobs=("hal_silabs", ".*", ".blobs_hal_silabs_ready"),
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d"),
    # No scratch partition in the board's flash layout (boot/image-0/image-1/storage
    # only), same shape as efr32mg24/nrf52 -- move/offset need no scratch, "swap" would.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    # The dataclass default ("uart0"/"uart1") doesn't exist here -- Silicon Labs
    # nodes are usartN/eusartN, and this board's console is usart1, not usart0.
    # Empty forces DTS auto-discovery, which always puts zephyr,console at UART0.
    uart_node_labels={},
    uart_valid_pins={"tx": _GPIO_MATRIX_PINS, "rx": _GPIO_MATRIX_PINS},
    spi_valid_pins={
        "clk": _GPIO_MATRIX_PINS,
        "mosi": _GPIO_MATRIX_PINS,
        "miso": _GPIO_MATRIX_PINS,
    },
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "efr32bg27", config, "EFR32BG27 support"
    )
    if CONF_COMMANDER_VERSION in config[CONF_ADVANCED] and sdk_name != "silabs":
        raise cv.Invalid(
            f"'{CONF_COMMANDER_VERSION}' only applies with framework: type: silabs "
            f"(current: {sdk_name!r})",
            [CONF_ADVANCED, CONF_COMMANDER_VERSION],
        )
    set_core_data(
        VARIANT_NAME,
        config[CONF_BOARD],
        BOOTLOADER_MCUBOOT,
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
        zephyr_setup_preferences,
    )

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_EFR32BG27")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "EFR32BG27")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # Same reasoning as efr32mg24/nrf52: xg27_dk2602a's board DTS hardcodes the
    # boot/image-0/image-1/storage partition layout and `zephyr,code-partition =
    # &slot0_partition` unconditionally (not gated behind any Kconfig), so the app
    # can only ever run via MCUboot jumping to slot0 -- this is required to boot at
    # all, regardless of whether OTA is configured.
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)

    # The sensor mezzanine's power switch (sw_sensor_enable) only declares
    # `regulator-always-on`, which stops a *consumer* disabling it again but
    # doesn't turn it on itself -- with no consumer, the whole I2C bus stays
    # unpowered (confirmed: every address timed out). regulator-boot-on forces
    # it on at init instead. Guarded to this board; a custom board_source:
    # board wouldn't have this node.
    if config[CONF_BOARD].partition("@")[0] == "xg27_dk2602a":
        zephyr_add_prj_conf("REGULATOR", True)
        zephyr_add_overlay("&sw_sensor_enable { regulator-boot-on; };")
