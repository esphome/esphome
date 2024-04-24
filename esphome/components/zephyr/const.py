import esphome.codegen as cg

ZEPHYR_VARIANT_GENERIC = "generic"
ZEPHYR_VARIANT_NRF_SDK = "nrf-sdk"
KEY_ZEPHYR = "zephyr"
KEY_PRJ_CONF = "prj_conf"
KEY_OVERLAY = "overlay"
KEY_BOOTLOADER = "bootloader"
BOOTLOADER_MCUBOOT = "mcuboot"

zephyr_ns = cg.esphome_ns.namespace("zephyr")
