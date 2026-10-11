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
    CONF_RUNNER,
    ZEPHYR_VARIANT_ESP32_C5,
)
from ..partitions import BootLayout
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

# qualify_board() expands this to "esp32c5_devkitc/esp32c5/hpcore" via soc=/qualifier= below.
_DEFAULT_BOARD = "esp32c5_devkitc"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

# https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/esp32c5-pinctrl.h
# GPIO0-28 minus 16-22 (SPI flash); same set for every free-mux signal (UART, SPI).
_GPIO_MATRIX_PINS = frozenset(range(29)) - frozenset(range(16, 23))

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_ESP32_C5
VARIANT = ZephyrVariant(
    # ROM loads the first image at 0x2000 (espressif/partitions_0x2000_*.dtsi).
    boot=BootLayout(0x2000),
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="esp32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    toolchain="riscv64-zephyr-elf",
    # See esp32_h2.py's blobs= comment. Sentinel shared with esp32_h2/esp32_c6 (same SDK).
    blobs=("hal_espressif", ".*", ".blobs_hal_espressif_ready"),
    transports=frozenset({"wifi", "ble", "openthread"}),
    transport_drivers={"wifi": ("WIFI_ESP32", "wifi")},
    soc="esp32c5",
    qualifier="hpcore",
    swap_methods=frozenset({"scratch", "move", "direct", "offset"}),
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32c5/include/soc/adc_channel.h
    adc1_channel_map={1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5},
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
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "esp32_c5", config, "mainline ESP32-C5 support"
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

    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_ESP32_C5")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP32C5")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
    # sysbuild's own BOOT_SIGNATURE_TYPE choice overrides a per-image setting.
    zephyr_add_sysbuild_conf("BOOT_SIGNATURE_TYPE_ECDSA_P256", True)
