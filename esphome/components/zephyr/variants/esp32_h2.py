import esphome.codegen as cg
from esphome.const import CONF_BOARD, KEY_FRAMEWORK_VERSION, ThreadModel, Toolchain
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_ESP32_H2
from ..dts_lookup import get_i2c_pinctrl_esp32
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# Bare name; qualify_board() expands it to "esp32h2_devkitm/esp32h2" using soc= below.
_DEFAULT_BOARD = "esp32h2_devkitm"

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/esp32h2-pinctrl.h
# No GPIO 6, 7, 15-21 (reserved for flash/RF); identical set for tx and rx on this variant.
_UART_VALID_PINS = frozenset(
    {0, 1, 2, 3, 4, 5, 8, 9, 10, 11, 12, 13, 14, 22, 23, 24, 25, 26, 27}
)

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_ESP32_H2
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    family="esp32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    # Zephyr's hal_espressif blob check verifies the HAL's entire manifest once BLE is
    # enabled, not just this chip's -- a chip-specific regex fails the same way, so fetch
    # everything. Sentinel shared with esp32_c6 (same SDK) so it only runs once.
    blobs=("hal_espressif", ".*", ".blobs_hal_espressif_ready"),
    pinctrl_extractors={"i2c": get_i2c_pinctrl_esp32},
    transports=frozenset({"ble", "openthread"}),
    soc="esp32h2",
    # offset excluded: upstream's BOOT_PREFER_SWAP_OFFSET requires !SOC_FAMILY_ESPRESSIF_ESP32.
    swap_methods=frozenset({"scratch", "move"}),
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32h2/include/soc/adc_channel.h
    adc1_channel_map={1: 0, 2: 1, 3: 2, 4: 3, 5: 4},
    uart_valid_pins={"tx": _UART_VALID_PINS, "rx": _UART_VALID_PINS},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    version_str, framework_ver = resolve_framework_version(
        VARIANT, "esp32_h2", config, "mainline ESP32-H2 support"
    )
    set_core_data(
        VARIANT_NAME, config[CONF_BOARD], BOOTLOADER_MCUBOOT, framework_ver, config
    )
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_ESP32_H2")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP32H2")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
