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
    ZEPHYR_VARIANT_ESP32,
)
from ..dts_lookup import get_i2c_pinctrl_esp32
from . import (
    MAINLINE,
    ZephyrVariant,
    qualify_board,
    resolve_framework_version,
    set_core_data,
)

_LOGGER = logging.getLogger(__name__)

# qualify_board() expands this to "esp32_devkitc/esp32/procpu" via soc=/qualifier= below.
_DEFAULT_BOARD = "esp32_devkitc"

_ADVANCED_SCHEMA = ADVANCED_SCHEMA

# Registry entries — collected by variants/__init__.py
VARIANT_NAME = ZEPHYR_VARIANT_ESP32
VARIANT = ZephyrVariant(
    sdk=MAINLINE,
    sdk_name="zephyr",
    family="esp32",
    valid_toolchains=(Toolchain.SDK_ZEPHYR,),
    # Original ESP32 is Xtensa (dual-core PRO_CPU/APP_CPU), unlike H2/C6's RISC-V --
    # a completely separate Zephyr SDK toolchain.
    toolchain="xtensa-espressif_esp32_zephyr-elf",
    # See esp32_h2.py's blobs= comment. Sentinel shared with esp32_h2/esp32_c6 (same SDK).
    blobs=("hal_espressif", ".*", ".blobs_hal_espressif_ready"),
    pinctrl_extractors={"i2c": get_i2c_pinctrl_esp32},
    transports=frozenset({"wifi", "ble"}),
    transport_drivers={"wifi": ("WIFI_ESP32", "wifi")},
    soc="esp32",
    # Original ESP32 is dual-core Xtensa (PRO_CPU/APP_CPU), not a HP/LP asymmetric pair
    # like esp32_c6 -- Zephyr's WIFI_ESP32 driver requires `!SMP` (single scheduled core),
    # so only the procpu image is built, mirroring esp32_c6's hpcore-only build.
    qualifier="procpu",
    # offset excluded: upstream's BOOT_PREFER_SWAP_OFFSET requires !SOC_FAMILY_ESPRESSIF_ESP32.
    swap_methods=frozenset({"scratch", "move", "direct"}),
    # https://github.com/espressif/esp-idf/blob/master/components/soc/esp32/include/soc/adc_channel.h
    adc1_channel_map={36: 0, 37: 1, 38: 2, 39: 3, 32: 4, 33: 5, 34: 6, 35: 7},
    # https://github.com/zephyrproject-rtos/zephyr/blob/main/include/zephyr/dt-bindings/pinctrl/esp32-pinctrl.h
    # No GPIO 24, 28-31 (SPI flash pins, not exposed via UART0/1_{TX,RX}_GPIO* macros).
    # TX additionally excludes GPIO34-39: input-only pins on original ESP32, no output driver.
    uart_valid_pins={
        "tx": frozenset(
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                17,
                18,
                19,
                20,
                21,
                22,
                23,
                25,
                26,
                27,
                32,
                33,
            }
        ),
        "rx": frozenset(
            {
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                17,
                18,
                19,
                20,
                21,
                22,
                23,
                25,
                26,
                27,
                32,
                33,
                34,
                35,
                36,
                37,
                38,
                39,
            }
        ),
    },
)


def config_schema(config: ConfigType) -> ConfigType:
    # Zephyr's SMP support for Xtensa ESP32 is opt-in via kconfig_options: CONFIG_SMP, and
    # WIFI_ESP32 requires SMP off regardless -- only warn when APP_CPU is really unused.
    if not config.get(CONF_KCONFIG_OPTIONS, {}).get("CONFIG_SMP"):
        _LOGGER.warning(
            "Original ESP32 is dual-core, but only one core (PRO_CPU) is used under Zephyr by "
            "default -- set kconfig_options: CONFIG_SMP: true to enable the second core (APP_CPU)."
        )
    config = dict(config)
    if CONF_BOARD not in config:
        config[CONF_BOARD] = _DEFAULT_BOARD
    config[CONF_ADVANCED] = _ADVANCED_SCHEMA(config.get(CONF_ADVANCED, {}))
    config[CONF_BOARD] = qualify_board(VARIANT, config[CONF_BOARD])
    _, framework_ver, sdk_name, _ = resolve_framework_version(
        VARIANT, "esp32", config, "mainline ESP32 support"
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
    from .. import zephyr_add_prj_conf, zephyr_setup_preferences, zephyr_to_code

    zephyr_to_code(config)
    cg.add_build_flag("-DUSE_ZEPHYR_VARIANT_ESP32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_define("ESPHOME_VARIANT", "ESP32")
    cg.add_define(ThreadModel.SINGLE)
    zephyr_setup_preferences()
    zephyr_add_prj_conf("REBOOT", True)
    # get_mac_address_raw() (zephyr/core.cpp) reads the efuse MAC via hwinfo_get_device_id().
    zephyr_add_prj_conf("HWINFO", True)
