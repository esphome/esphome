from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_add_overlay


def zephyr_add_cdc_acm(config):
    zephyr_add_prj_conf("USB_DEVICE_STACK", True)
    zephyr_add_prj_conf("USB_CDC_ACM", True)
    # prevent device to go to susspend
    zephyr_add_prj_conf("USB_DEVICE_REMOTE_WAKEUP", False)
    zephyr_add_overlay(
        """
&zephyr_udc0 {
    cdc_acm_uart0: cdc_acm_uart0 {
        compatible = "zephyr,cdc-acm-uart";
    };
};
"""
    )
