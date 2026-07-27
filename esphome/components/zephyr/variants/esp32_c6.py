import esphome.codegen as cg
from esphome.const import CONF_BOARD, KEY_FRAMEWORK_VERSION, ThreadModel, Toolchain
from esphome.types import ConfigType

from ..const import BOOTLOADER_MCUBOOT, ZEPHYR_VARIANT_ESP32_C6
from ..dts_lookup import get_i2c_pinctrl_esp32
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# qualify_board() expands this to "esp32c6_devkitc/esp32c6/hpcore" via soc=/qualifier= below.
_DEFAULT_BOARD = "esp32c6_devkitc"

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/esp32c6-pinctrl.h
# GPIO0-23, no gaps; identical set for tx and rx on this variant.
_UART_VALID_PINS = frozenset(range(24))

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_ESP32_C6
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    family="esp32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    # See esp32_h2.py's blobs= comment. Sentinel shared with esp32_h2 (same SDK).
    blobs=("hal_espressif", ".*", ".blobs_hal_espressif_ready"),
    pinctrl_extractors={"i2c": get_i2c_pinctrl_esp32},
    transports=frozenset({"wifi", "ble", "openthread"}),
    transport_drivers={"wifi": ("WIFI_ESP32", "wifi")},
    soc="esp32c6",
    qualifier="hpcore",
    # offset excluded: upstream's BOOT_PREFER_SWAP_OFFSET requires !SOC_FAMILY_ESPRESSIF_ESP32.
    swap_methods=frozenset({"scratch", "move"}),
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c6/include/soc/adc_channel.h
    adc1_channel_map={0: 0, 1: 1, 2: 2, 3: 3, 4: 4, 5: 5, 6: 6},
    uart_valid_pins={"tx": _UART_VALID_PINS, "rx": _UART_VALID_PINS},
)


def config_schema(config: ConfigType) -> ConfigType:
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    version_str, framework_ver = resolve_framework_version(
        VARIANT, "esp32_c6", config, "mainline ESP32-C6 support"
    )
    set_core_data(
        VARIANT_NAME, config[CONF_BOARD], BOOTLOADER_MCUBOOT, framework_ver, config
    )
    config[KEY_FRAMEWORK_VERSION] = version_str
    return config


async def to_code(config: ConfigType) -> None:
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_ESP32_C6")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP32C6")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
