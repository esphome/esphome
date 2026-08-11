from typing import Final

import esphome.codegen as cg

BOOTLOADER_MCUBOOT = "mcuboot"

KEY_BOOTLOADER: Final = "bootloader"
KEY_EXTRA_BUILD_FILES: Final = "extra_build_files"
KEY_OVERLAY: Final = "overlay"
KEY_OVERLAY_BUILDER: Final = "overlay_builder"
KEY_PM_STATIC: Final = "pm_static"
KEY_KCONFIG: Final = "kconfig"
KEY_PRJ_CONF: Final = "prj_conf"
KEY_SYSBUILD_CONF: Final = "sysbuild_conf"
KEY_ZEPHYR = "zephyr"
KEY_BOARD: Final = "board"
KEY_BOARD_ROOT: Final = "board_root"
KEY_USER: Final = "user"
KEY_FRAMEWORK_TYPE: Final = "framework_type"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
CdcAcm = zephyr_ns.class_("CdcAcm", cg.Component)
CONF_CDC_ACM = "cdc_acm"
CONF_BOARD_SOURCE = "board_source"
CONF_KCONFIG_OPTIONS = "kconfig_options"
CONF_OVERLAYS = "overlays"
CONF_WEST_VERSION = "west_version"
CONF_NINJA_VERSION = "ninja_version"
CONF_SNIPPETS = "snippets"
KEY_SNIPPETS: Final = "snippets"

# zephyr: version: "recommended" -- explicit alias for the variant's default_version,
# same as omitting `version:` entirely.
VERSION_RECOMMENDED: Final = "recommended"

ZEPHYR_VARIANT_ESP32 = "ESP32"
ZEPHYR_VARIANT_ESP32_H2 = "ESP32H2"
ZEPHYR_VARIANT_ESP32_C6 = "ESP32C6"
ZEPHYR_VARIANT_ESP32_C5 = "ESP32C5"
ZEPHYR_VARIANT_NATIVE_SIM = "NATIVESIM"
ZEPHYR_VARIANT_NRF52 = "NRF52"
ZEPHYR_VARIANT_NRF54L15 = "NRF54L15"
ZEPHYR_VARIANT_NRF54LM20A = "NRF54LM20A"
ZEPHYR_VARIANT_EFR32MG24 = "EFR32MG24"
ZEPHYR_VARIANT_RP2040 = "RP2040"
ZEPHYR_VARIANT_RP2350 = "RP2350"

ZephyrI2CEmulator = zephyr_ns.class_("ZephyrI2CEmulator", cg.Component)
