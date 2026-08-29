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

from ..const import (
    ADVANCED_SCHEMA,
    BOOTLOADER_MCUBOOT,
    CONF_RUNNER,
    ZEPHYR_VARIANT_EFR32MG24,
)
from ..dts_lookup import get_i2c_pinctrl_silabs
from . import (
    MAINLINE,
    SILABS,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# xG24-EK2703A (PCB2703A), Silicon Labs' EFR32xG24 Explorer Kit. Bare name; no
# SoC/qualifier segment needed -- upstream's board.yml declares a single SoC/core
# for this board, so qualify_board() is a no-op here (soc= is left unset below).
_DEFAULT_BOARD = "xg24_ek2703a"

# framework: type: silabs only -- overrides commander_setup.py's pinned default. Its own
# release cadence is independent of the silabs SDK version, same reasoning as
# west_version:/ninja_version: vs. the Zephyr version.
CONF_COMMANDER_VERSION = "commander_version"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA.extend(
    {
        cv.Optional(CONF_COMMANDER_VERSION): cv.string_strict,
    }
)

# GPIO -> Silicon Labs IADC analog-input macro name. Fully regular fixed-function
# silicon mapping (unlike nRF52's sparse datasheet AIN0-AIN7 table): every GPIO pin
# PA0..PD15 has its own IADC_INPUT_P<port><pin> macro, confirmed against
# zephyr/dt-bindings/adc/silabs-adc.h (e.g. IADC_INPUT_PC4 = 0xa4).
_ADC_AIN_MAP = {p: f"IADC_INPUT_P{chr(ord('A') + p // 16)}{p % 16}" for p in range(64)}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_EFR32MG24
VARIANT = ZephyrVariant(
    # Mainline stays default; Silicon Labs' vendor SDK (SILABS) is available as an alt
    # (framework: type: silabs) pending real hardware testing.
    sdk=MAINLINE,
    sdk_name="zephyr",
    alt_sdks={"silabs": SILABS},
    family="silabs",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # BLE always available (bt_hci_silabs HCI node enabled upstream). OpenThread also
    # possible via the ieee802154 driver added upstream 2026-02-12 (commit bd89985e6,
    # "drivers: ieee802154: Add support of 802.15.4 for EFR xG24") -- not yet in any
    # tagged Zephyr release as of v4.4.1, so it needs `framework: source: {type: git,
    # ref: main}` for now. No Zigbee support in this fork yet either way.
    transports=frozenset({"ble", "openthread"}),
    # Zephyr's hal_silabs blob check verifies the HAL's entire manifest once BT or
    # 802.15.4 is enabled, not just this chip's -- e.g. the SiWG917 Wi-Fi/BT combo's
    # firmware blob, unrelated to EFR32MG24 -- so fetch everything. Same reasoning as
    # esp32_h2/esp32_c6's hal_espressif blobs. IEEE802154_SILABS_EFR32 also directly
    # `depends on ZEPHYR_HAL_SILABS_MODULE_BLOBS`, so this is required for OpenThread
    # too, not just BLE.
    blobs=("hal_silabs", ".*", ".blobs_hal_silabs_ready"),
    pinctrl_extractors={"i2c": get_i2c_pinctrl_silabs},
    gpio_port_width=16,
    gpio_port_labels=("a", "b", "c", "d"),
    # No scratch partition in the board's flash layout (boot/image-0/image-1/storage
    # only), same shape as nrf52 -- move/offset need no scratch, "swap" would.
    swap_methods=frozenset({"move", "offset"}),
    adc_ain_map=_ADC_AIN_MAP,
    # Only usart0 exists at SoC level on this board -- no usart1/UART1.
    uart_node_labels={"UART0": "usart0"},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "efr32mg24", config, "EFR32MG24 support"
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
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import (
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_setup_preferences,
        zephyr_to_code,
    )

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_EFR32MG24")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "EFR32MG24")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    zephyr_add_prj_conf("HWINFO", True)

    # Same reasoning as nrf52: xg24_ek2703a's board DTS hardcodes the
    # boot/image-0/image-1/storage partition layout and `zephyr,code-partition =
    # &slot0_partition` unconditionally (not gated behind any Kconfig), so the app
    # can only ever run via MCUboot jumping to slot0 -- this is required to boot at
    # all, regardless of whether OTA is configured.
    zephyr_add_sysbuild_conf("BOOTLOADER_MCUBOOT", True)
    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
