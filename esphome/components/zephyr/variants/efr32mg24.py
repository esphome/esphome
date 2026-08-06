import esphome.codegen as cg
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_SOURCE,
    KEY_FRAMEWORK_VERSION,
    ThreadModel,
    Toolchain,
)
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_EFR32MG24
from ..dts_lookup import get_i2c_pinctrl_silabs
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# xG24-EK2703A (PCB2703A), Silicon Labs' EFR32xG24 Explorer Kit. Bare name; no
# SoC/qualifier segment needed -- upstream's board.yml declares a single SoC/core
# for this board, so qualify_board() is a no-op here (soc= is left unset below).
_DEFAULT_BOARD = "xg24_ek2703a"

# GPIO -> Silicon Labs IADC analog-input macro name. Fully regular fixed-function
# silicon mapping (unlike nRF52's sparse datasheet AIN0-AIN7 table): every GPIO pin
# PA0..PD15 has its own IADC_INPUT_P<port><pin> macro, confirmed against
# zephyr/dt-bindings/adc/silabs-adc.h (e.g. IADC_INPUT_PC4 = 0xa4).
_ADC_AIN_MAP = {p: f"IADC_INPUT_P{chr(ord('A') + p // 16)}{p % 16}" for p in range(64)}

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_EFR32MG24
VARIANT = ZephyrVariant(
    # No Silicon Labs vendor SDK equivalent to Nordic's NCS -- mainline Zephyr is the
    # only SDK carrying silabs board/HAL support.
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="silabs",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="arm-zephyr-eabi",
    # BLE only: the xg24_ek2703a's bt_hci_silabs HCI node is enabled upstream, but this
    # Zephyr tree has no ieee802154 driver for Silicon Labs Series 2 -- OpenThread and
    # Zigbee aren't feasible yet regardless of radio capability.
    transports=frozenset({"ble"}),
    # Zephyr's hal_silabs blob check verifies the HAL's entire manifest once BT is
    # enabled, not just this chip's -- e.g. the SiWG917 Wi-Fi/BT combo's firmware
    # blob, unrelated to EFR32MG24 -- so fetch everything. Same reasoning as
    # esp32_h2/esp32_c6's hal_espressif blobs.
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
    version_str, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "efr32mg24", config, "EFR32MG24 support"
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
    # RSA-2048 (mcuboot's default) is code-size heavy; ECDSA-P256 has a much
    # smaller footprint. Same tradeoff nrf52/nrf54l15 make -- strictly smaller either way.
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_RSA", False, image="mcuboot")
    zephyr_add_prj_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True, image="mcuboot")
