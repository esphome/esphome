import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_add_overlay

fota_ns = cg.esphome_ns.namespace("fota")
FOTAComponent = fota_ns.class_("FOTAComponent", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(FOTAComponent),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_nrf52,
    cv.only_with_zephyr,
)

bt = True
cdc = True


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    zephyr_add_prj_conf("NET_BUF", True)
    zephyr_add_prj_conf("ZCBOR", True)
    zephyr_add_prj_conf("MCUMGR", True)
    zephyr_add_prj_conf("BOOTLOADER_MCUBOOT", True)

    zephyr_add_prj_conf("IMG_MANAGER", True)
    zephyr_add_prj_conf("STREAM_FLASH", True)
    zephyr_add_prj_conf("FLASH_MAP", True)
    zephyr_add_prj_conf("FLASH", True)
    zephyr_add_prj_conf("MCUBOOT_SHELL", True)
    zephyr_add_prj_conf("SHELL", True)
    zephyr_add_prj_conf("SYSTEM_WORKQUEUE_STACK_SIZE", 2304)
    zephyr_add_prj_conf("MAIN_STACK_SIZE", 2048)

    zephyr_add_prj_conf("MCUMGR_GRP_IMG", True)
    zephyr_add_prj_conf("MCUMGR_GRP_OS", True)

    # echo
    zephyr_add_prj_conf("MCUMGR_GRP_OS_ECHO", True)
    # for Android-nRF-Connect-Device-Manager
    zephyr_add_prj_conf("MCUMGR_GRP_OS_INFO", True)
    zephyr_add_prj_conf("MCUMGR_GRP_OS_BOOTLOADER_INFO", True)
    zephyr_add_prj_conf("MCUMGR_GRP_OS_MCUMGR_PARAMS", True)
    # bt update fails without this
    zephyr_add_prj_conf("MCUMGR_GRP_SHELL", True)
    # make MTU bigger and other things
    zephyr_add_prj_conf("NCS_SAMPLE_MCUMGR_BT_OTA_DFU_SPEEDUP", True)

    # zephyr_add_prj_conf("MCUMGR_TRANSPORT_LOG_LEVEL_DBG", True)

    if bt:
        zephyr_add_prj_conf("BT", True)
        zephyr_add_prj_conf("BT_PERIPHERAL", True)
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT", True)
        # fix corrupted data during ble transport
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_BT_REASSEMBLY", True)
    if cdc:
        zephyr_add_prj_conf("MCUMGR_TRANSPORT_UART", True)
        zephyr_add_prj_conf("USB_DEVICE_STACK", True)
        zephyr_add_prj_conf("BASE64", True)
        zephyr_add_prj_conf("CONSOLE", True)
        # needed ?
        zephyr_add_prj_conf("SERIAL", True)
        zephyr_add_prj_conf("UART_LINE_CTRL", True)
        zephyr_add_overlay(
            """
/ {
    chosen {
        zephyr,uart-mcumgr = &cdc_acm_uart0;
    };
};

&zephyr_udc0 {
    cdc_acm_uart0: cdc_acm_uart0 {
        compatible = "zephyr,cdc-acm-uart";
    };
};
"""
        )

    await cg.register_component(var, config)
