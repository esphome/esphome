import esphome.codegen as cg

KEY_ZEPHYR = "zephyr"
KEY_PRJ_CONF = "prj_conf"
KEY_OVERLAY = "overlay"
KEY_BOOTLOADER = "bootloader"
KEY_EXTRA_BUILD_FILES = "extra_build_files"
KEY_PATH = "path"

BOOTLOADER_MCUBOOT = "mcuboot"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
