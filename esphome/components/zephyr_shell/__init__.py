import esphome.config_validation as cv
from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_add_overlay

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    zephyr_add_prj_conf("SHELL", True)
    # zephyr_ble_server
    zephyr_add_prj_conf("BT_SHELL", True)
    # ota
    zephyr_add_prj_conf("MCUBOOT_SHELL", True)
    # i2c
    zephyr_add_prj_conf("I2C_SHELL", True)
    # zigbee
    zephyr_add_prj_conf("ZIGBEE_SHELL", True)
    # select uart for shell
    zephyr_add_overlay(
        """
/ {
    chosen {
        zephyr,shell-uart = &cdc_acm_uart1;
    };
};
&zephyr_udc0 {
    cdc_acm_uart1: cdc_acm_uart1 {
        compatible = "zephyr,cdc-acm-uart";
    };
};
"""
    )
