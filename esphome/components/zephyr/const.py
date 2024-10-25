import esphome.codegen as cg

BOOTLOADER_MCUBOOT = "mcuboot"

KEY_BOOTLOADER = "bootloader"
KEY_EXTRA_BUILD_FILES = "extra_build_files"
KEY_OVERLAY = "overlay"
KEY_PATH = "path"
KEY_PRJ_CONF = "prj_conf"
KEY_ZEPHYR = "zephyr"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
