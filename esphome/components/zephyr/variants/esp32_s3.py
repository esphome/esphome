import logging

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
    CONF_KCONFIG_OPTIONS,
    CONF_RUNNER,
    ZEPHYR_VARIANT_ESP32_S3,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

# qualify_board() expands this to "esp32s3_devkitc/esp32s3/procpu" via soc=/qualifier= below.
_DEFAULT_BOARD = "esp32s3_devkitc"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/esp32s3-pinctrl.h
# GPIO0-21 and 33-48; 22-25 don't exist, 26-32 are SPI flash. Same set for every
# free-mux signal (UART, SPI).
_GPIO_MATRIX_PINS = frozenset(range(22)) | frozenset(range(33, 49))

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_ESP32_S3
VARIANT = ZephyrVariant(
    # ROM loads the first image at 0x0 (espressif/partitions_0x0_*.dtsi).
    boot=BootLayout(0x0),
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="esp32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="xtensa-espressif_esp32s3_zephyr-elf",
    # See esp32_h2.py's blobs= comment. Sentinel shared with the other esp32-family
    # variants (same SDK).
    blobs=("hal_espressif", ".*", ".blobs_hal_espressif_ready"),
    transports=frozenset({"wifi", "ble"}),
    transport_drivers={"wifi": ("WIFI_ESP32", "wifi")},
    soc="esp32s3",
    # Dual-core Xtensa LX7 like the original ESP32; WIFI_ESP32 requires `!SMP`, so
    # only the procpu image is built.
    qualifier="procpu",
    swap_methods=frozenset({"scratch", "move", "offset"}),
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32s3/include/soc/adc_channel.h
    adc1_channel_map={pin: pin - 1 for pin in range(1, 11)},
    uart_node_labels={},
    uart_valid_pins={"tx": _GPIO_MATRIX_PINS, "rx": _GPIO_MATRIX_PINS},
    spi_valid_pins={
        "clk": _GPIO_MATRIX_PINS,
        "mosi": _GPIO_MATRIX_PINS,
        "miso": _GPIO_MATRIX_PINS,
    },
)


def config_schema(config: ConfigType) -> ConfigType:
    if not config.get(CONF_KCONFIG_OPTIONS, {}).get("CONFIG_SMP"):
        _LOGGER.warning(
            "ESP32-S3 is dual-core, but under Zephyr only one core (PRO_CPU) is "
            "used, unlike ESP-IDF builds."
        )
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "esp32_s3", config, "mainline ESP32-S3 support"
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
        zephyr_add_prj_conf,
        zephyr_add_sysbuild_conf,
        zephyr_setup_preferences,
    )

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_ESP32_S3")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP32S3")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
