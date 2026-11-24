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
KEY_ZEPHYR = "zephyr"
KEY_BOARD: Final = "board"
KEY_SYSBUILD: Final = "sysbuild"
KEY_VARIANT: Final = "variant"
KEY_FRAMEWORK_TYPE: Final = "framework_type"
KEY_FAMILY: Final = "family"

# `framework: version: recommended`, the same as leaving `version:` out
VERSION_RECOMMENDED: Final = "recommended"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
