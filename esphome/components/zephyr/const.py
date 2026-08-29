from typing import Final

import esphome.codegen as cg
import esphome.config_validation as cv

BOOTLOADER_MCUBOOT = "mcuboot"

# advanced: bootloader: -- shared by every variant whose default board can boot
# without MCUboot (rp2040, rp2350, ra4m1, stm32l4), so MCUboot is opt-in rather than
# assumed.
CONF_BOOTLOADER = "bootloader"
BOOTLOADER_NONE = "none"

# advanced: runner: -- free-text west flash runner override. Valid names are
# board/SDK-specific and only knowable after CMake configure, so this is
# intentionally unvalidated here (see build_zephyr.log_available_runners()).
CONF_RUNNER = "runner"

# Base advanced: schema shared by every variant; extend() on extra per-variant keys.
ADVANCED_SCHEMA = cv.Schema({cv.Optional(CONF_RUNNER): cv.string_strict})

BOOTLOADER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_BOOTLOADER, default=BOOTLOADER_NONE): cv.one_of(
            BOOTLOADER_NONE, BOOTLOADER_MCUBOOT, lower=True
        ),
    }
)

KEY_BOOTLOADER: Final = "bootloader"
KEY_RUNNER: Final = "runner"
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
KEY_MODULE_REQUESTS: Final = "module_requests"
KEY_MODULE_OVERRIDES: Final = "module_overrides"

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
CONF_SINGLE_SLOT = "single_slot"
KEY_SINGLE_SLOT: Final = "single_slot"
CONF_SHIELDS = "shields"
CONF_SHIELD_SOURCE = "shield_source"
KEY_SHIELDS: Final = "shields"
KEY_SHIELD_ROOT: Final = "shield_root"
CONF_SNIPPET_SOURCE = "snippet_source"
KEY_SNIPPET_ROOT: Final = "snippet_root"
CONF_MODULES = "modules"

# zephyr: version: "recommended" -- explicit alias for the variant's default_version,
# same as omitting `version:` entirely.
VERSION_RECOMMENDED: Final = "recommended"

ZEPHYR_VARIANT_ESP32 = "ESP32"
ZEPHYR_VARIANT_ESP32_H2 = "ESP32H2"
ZEPHYR_VARIANT_ESP32_C6 = "ESP32C6"
ZEPHYR_VARIANT_ESP32_C5 = "ESP32C5"
ZEPHYR_VARIANT_ESP32_C3 = "ESP32C3"
ZEPHYR_VARIANT_NATIVE_SIM = "NATIVESIM"
ZEPHYR_VARIANT_NRF52 = "NRF52"
ZEPHYR_VARIANT_NRF54L15 = "NRF54L15"
ZEPHYR_VARIANT_NRF54LM20A = "NRF54LM20A"
ZEPHYR_VARIANT_EFR32MG24 = "EFR32MG24"
ZEPHYR_VARIANT_STM32L4 = "STM32L4"
ZEPHYR_VARIANT_STM32F4 = "STM32F4"
ZEPHYR_VARIANT_STM32WB55 = "STM32WB55"
ZEPHYR_VARIANT_STM32F1 = "STM32F1"
ZEPHYR_VARIANT_STM32U5 = "STM32U5"
ZEPHYR_VARIANT_RP2040 = "RP2040"
ZEPHYR_VARIANT_RP2350 = "RP2350"
ZEPHYR_VARIANT_RA4M1 = "RA4M1"

ZephyrI2CEmulator = zephyr_ns.class_("ZephyrI2CEmulator", cg.Component)

# Python-only marker interface (no C++ side, see MockObjClass.class_/inherits_from) shared by
# UARTComponent and ZephyrUartEmulator so `uart.write`'s `id:` can validate against either --
# defined here rather than in uart/const.py to avoid a zephyr <-> uart import cycle, since
# uart's __init__.py already imports this module.
ZephyrUartWriteTarget = zephyr_ns.class_("ZephyrUartWriteTarget")
ZephyrUartEmulator = zephyr_ns.class_(
    "ZephyrUartEmulator", cg.Component, ZephyrUartWriteTarget
)
