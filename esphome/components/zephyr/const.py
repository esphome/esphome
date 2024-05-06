import esphome.codegen as cg

ZEPHYR_VARIANT_GENERIC = "generic"
ZEPHYR_VARIANT_NRF_SDK = "nrf-sdk"

KEY_ZEPHYR = "zephyr"
KEY_PRJ_CONF = "prj_conf"
KEY_OVERLAY = "overlay"
KEY_BOOTLOADER = "bootloader"
KEY_EXTRA_BUILD_FILES = "extra_build_files"
KEY_PATH = "path"

BOOTLOADER_MCUBOOT = "mcuboot"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
