from typing import Final

import esphome.codegen as cg

BOOTLOADER_MCUBOOT = "mcuboot"

KEY_BOOTLOADER: Final = "bootloader"
KEY_EXTRA_BUILD_FILES: Final = "extra_build_files"
KEY_OVERLAY: Final = "overlay"
KEY_PM_STATIC: Final = "pm_static"
KEY_PRJ_CONF: Final = "prj_conf"
KEY_ZEPHYR = "zephyr"
KEY_BOARD: Final = "board"
KEY_USER: Final = "user"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
