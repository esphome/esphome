import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADVANCED,
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import ADVANCED_SCHEMA, CONF_RUNNER, ZEPHYR_VARIANT_SIWX917
from . import (
    MAINLINE,
    SILABS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# SiWx917-DK2605A (BRD2605A), Silicon Labs' SiWx917 Wi-Fi 6 + BLE 5.4 dev kit. Bare
# name; no SoC/qualifier segment needed -- upstream's board.yml declares a single
# SoC for this board, same shape as efr32mg24's xg24_ek2703a.
_DEFAULT_BOARD = "siwx917_dk2605a"

# framework: type: silabs only -- see efr32mg24.py's own CONF_COMMANDER_VERSION
# comment. This chip's real firmware-upgrade tooling also goes through Silicon
# Labs' "commander" CLI (Kconfig SIWX91X_SIGN_KEY/SIWX91X_MIC_KEY are both passed
# straight to `commander rps convert`; the same command connectedhomeip's SiWx917
# OTA port uses, github.com/project-chip/connectedhomeip/pull/74060).
CONF_COMMANDER_VERSION = "commander_version"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_COMMANDER_VERSION): cv.string_strict,
    }
)

# GPIO -> Silicon Labs SiWx91x ADC positive-input macro. Only 6 HP (High-Power
# domain) pins are wired to the ADC's single-ended inputs -- confirmed against
# zephyr/include/zephyr/dt-bindings/adc/silabs-siwx91x-adc.h. That header also
# defines ULPn macros for pins on the separate ULP GPIO controller, outside this
# variant's flat gpioa-d numbering (see gpio_port_width/gpio_port_labels below) --
# not wired up here, same deferral as UART/I2C further down.
_ADC_AIN_MAP = {
    25: "SIWX91X_ADC_INPUT_HP25",
    26: "SIWX91X_ADC_INPUT_HP26",
    27: "SIWX91X_ADC_INPUT_HP27",
    28: "SIWX91X_ADC_INPUT_HP28",
    29: "SIWX91X_ADC_INPUT_HP29",
    30: "SIWX91X_ADC_INPUT_HP30",
}

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/silabs/siwx91x-pinctrl.h
# Unlike EFR32's full crossbar (a per-pin macro *formula*, e.g. "{PREFIX}_{SIGNAL}_
# P{letter}{num}"), SiWx91x's HP GPIO mux only defines a macro for a handful of
# enumerated candidate pins per signal -- these are gspi0's, the chip's only SPI
# controller. Flat pin numbers equal the macros' own HPnn suffix (port*16+pin
# always reduces to nn for these macros, the same fact the ADC map above relies
# on), so the macro name itself is still derivable, just not by formula -- hence
# a lookup table (spi_pin_macros) rather than spi_valid_pins alone.
_SPI_CLK_MACROS = {p: f"GSPI_CLK_HP{p}" for p in (8, 25, 46, 52)}
_SPI_MOSI_MACROS = {p: f"GSPI_MOSI_HP{p}" for p in (6, 12, 27, 48, 57)}
_SPI_MISO_MACROS = {p: f"GSPI_MISO_HP{p}" for p in (11, 26, 47, 56)}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_SIWX917
VARIANT = ZephyrVariant(
    # Mainline stays default; Silicon Labs' vendor SDK (SILABS) is available as an
    # alt, same reasoning as efr32mg24 -- pending real hardware testing either way.
    sdk=MAINLINE,
    sdk_name="zephyr",
    alt_sdks={"silabs": SILABS},
    # Deliberately its own family, not efr32mg24's "silabs": EFR32 Series 2 and
    # SiWx91x are unrelated silicon sharing only a vendor name -- different
    # GPIO/pinmux (crossbar vs. fixed-function here) and a completely different ADC
    # IP with its own dt-bindings header and devicetree shape. Reusing "silabs"
    # would misdirect adc/sensor.py's family-keyed codegen at EFR32's IADC overlay
    # shape, which doesn't apply to this chip's ADC driver.
    family="silabs_siwx91x",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # Wi-Fi 6 + BLE 5.4 only -- no 802.15.4 radio on this chip.
    transports=frozenset({"wifi", "ble"}),
    # wifi0's DT node (and bt_hci0 for BLE) already default to `status = "okay"` on
    # this board -- WIFI_SILABS_SIWX91X/BT_SILABS_SIWX91X both `default y depends on
    # DT_HAS_..._ENABLED`, so this mostly re-states what's already on. Still
    # required: wifi/__init__.py raises if a "wifi"-transport variant has no
    # transport_drivers entry for it.
    transport_drivers={"wifi": ("WIFI_SILABS_SIWX91X", "wifi0")},
    # Same hal_silabs blob-check reasoning as efr32mg24's own comment about this
    # exact chip's firmware blob (that comment was written anticipating this
    # variant): Zephyr's hal_silabs blob check verifies the HAL's entire manifest
    # once BT or Wi-Fi is enabled, not just the active chip's, so fetch everything.
    blobs=("hal_silabs", ".*", ".blobs_hal_silabs_ready"),
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d"),
    # egpio0's 4 ports are declared uniformly ngpios=16 here (matching every other
    # lettered-port family) even though gpiod's own devicetree node declares only
    # ngpios=10 -- flat pins 58-63 don't physically exist (confirmed against
    # dts/arm/silabs/siwg917.dtsi's gpiod node and its `silabs,pads` property, which
    # lists 0xff -- not implemented -- for those 6 trailing entries). Per
    # variants/__init__.py's own warning about gpio_node_prefix: this is silent at
    # compile time -- a pin in that range just resolves to a null GPIO device and
    # fails at runtime, not at `esphome config`/build time.
    #
    # The ULP GPIO controller (ulpgpio, 12 pins) and UULP GPIO controller
    # (uulpgpio, 5 pins -- wired to the board's SW0 button) are separate
    # controllers outside this flat numbering entirely, not covered by this
    # variant yet (same deferral as the ADC map above).
    adc_ain_map=_ADC_AIN_MAP,
    # UART/I2C pin remapping intentionally left unsupported for now
    # (uart_valid_pins*/i2c_valid_pins_by_instance all stay at their empty class
    # default): this board's console UART (ulpuart) and default I2C bus (ulpi2c)
    # both live on the ULP GPIO domain this variant doesn't model (see above),
    # while the two HP-domain UARTs (uart0/uart1) and I2C buses (i2c0/i2c1) all
    # ship `status = "disabled"` by default with no board-level default pinctrl to
    # fall back on. uart_node_labels is set to {} (dynamic discovery) rather than
    # the usual {"UART0": "uart0", "UART1": "uart1"} class default -- this board's
    # real `zephyr,console` is ulpuart, not uart0, so that static default would be
    # wrong (confirmed live: without this override, dts_lookup.py hardcodes
    # UART0=uart0 and warns that it doesn't match the board's actual console).
    uart_node_labels={},
    spi_valid_pins={
        "clk": frozenset(_SPI_CLK_MACROS),
        "mosi": frozenset(_SPI_MOSI_MACROS),
        "miso": frozenset(_SPI_MISO_MACROS),
    },
    spi_pin_macros={
        "clk": _SPI_CLK_MACROS,
        "mosi": _SPI_MOSI_MACROS,
        "miso": _SPI_MISO_MACROS,
    },
    # This SoC's own dtsi names its pinctrl controller "pinctrl0", not the
    # "pinctrl" every other family wired up so far uses.
    pinctrl_node_label="pinctrl0",
    # No MCUboot/OTA support: the board's flash0 partitions are hardcoded to a
    # single `code_partition` (`zephyr,code-partition` set unconditionally, no
    # slot0/slot1 dual-bank layout, no bootloader:-selectable Kconfig gate) --
    # confirmed against boards/silabs/dev_kits/siwx917_dk2605a/siwx917_dk2605a.dts.
    # This chip's real OTA path is Silicon Labs' own RPS (Recoverable Partition
    # Scheme) mechanism instead, layered on the ROM/Security bootloader via the
    # `sl_si91x_fwup_*` firmware-upgrade APIs (Kconfig SIWX91X_FIRMWARE_UPGRADE) --
    # not compatible with Zephyr's generic MCUboot/IMG_MANAGER assumptions this
    # platform's own OTA integration relies on (see connectedhomeip's own draft
    # port explaining exactly this gap: github.com/project-chip/connectedhomeip/
    # pull/74060). swap_methods stays at its empty class default, and no
    # bootloader: option is exposed at all (same shape as native_sim) -- unlike
    # rp2040/efr32mg24, there's no upstream MCUboot-shaped board target to opt into
    # here without a fully custom partition overlay this fork doesn't provide yet.
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "siwx917", config, "SiWx917 support"
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
        "",
        framework_ver,
        config,
        framework_type=sdk_name,
        sdk_source=config[CONF_FRAMEWORK].get(CONF_SOURCE),
        runner=config[CONF_ADVANCED].get(CONF_RUNNER),
    )
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_SIWX917")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "SIWX917")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # This board's own defconfig (siwx917_dk2605a_defconfig) is unusually minimal --
    # unlike every other board this platform targets, it doesn't set CONFIG_GPIO=y
    # itself (confirmed on real hardware: without this, DEVICE_DT_GET() on gpioa/
    # gpiob links but the actual device is never compiled in -- undefined reference
    # to __device_dts_ord_*, not a DTS-level failure).
    zephyr_add_prj_conf("GPIO", True)
    # No real HWINFO backend for this chip in mainline Zephyr yet (the only Silicon
    # Labs one, drivers/hwinfo/hwinfo_silabs_series2.c, depends on
    # SOC_FAMILY_SILABS_S2 -- EFR32 Series 2, not SOC_FAMILY_SILABS_SIWX91X). Still
    # enabled: get_mac_address_raw() (zephyr/core.cpp) unconditionally calls
    # hwinfo_get_device_id() for this family, which needs CONFIG_HWINFO just to link
    # (its weak default implementation returns -ENOSYS with no backend selected,
    # which get_mac_address_raw() already treats as "no MAC available" and zeroes).
    # Real hardware testing may yet find a genuine ID/MAC source via the WiseConnect
    # NWP instead -- until then, this variant's MAC is all-zero.
    zephyr_add_prj_conf("HWINFO", True)
