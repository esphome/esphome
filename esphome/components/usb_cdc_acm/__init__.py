import esphome.codegen as cg
from esphome.components import esp32, uart
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.components.esp32.const import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@kbx81"]
AUTO_LOAD = ["uart"]
DEPENDENCIES = ["tinyusb"]

CONF_INTERFACES = "interfaces"
CONF_USB_RX_BUFFER_SIZE = "usb_rx_buffer_size"
CONF_USB_TX_BUFFER_SIZE = "usb_tx_buffer_size"

usb_cdc_acm_ns = cg.esphome_ns.namespace("usb_cdc_acm")
USBCDCACMComponent = usb_cdc_acm_ns.class_("USBCDCACMComponent", cg.Component)
USBCDCACMInstance = usb_cdc_acm_ns.class_("USBCDCACMInstance", uart.UARTComponent)


# Schema for individual CDC ACM interface instances
INTERFACE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(USBCDCACMInstance),
    }
)

# Main component schema
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBCDCACMComponent),
            cv.Optional(CONF_USB_RX_BUFFER_SIZE, default=256): cv.uint16_t,
            cv.Optional(CONF_USB_TX_BUFFER_SIZE, default=256): cv.uint16_t,
            cv.Optional(CONF_INTERFACES, default=[{}]): cv.All(
                cv.ensure_list(INTERFACE_SCHEMA),
                cv.Length(min=1, max=2),  # At least 1, at most 2 interfaces
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Create and register interface instances
    for interface_index, interface_conf in enumerate(config[CONF_INTERFACES]):
        interface = cg.new_Pvariable(interface_conf[CONF_ID])
        cg.add(interface.set_parent(var))
        cg.add(interface.set_interface_number(interface_index))
        cg.add(var.add_interface(interface))

    # Configure TinyUSB with the correct number of CDC interfaces
    num_interfaces = len(config[CONF_INTERFACES])
    add_idf_sdkconfig_option("CONFIG_TINYUSB_CDC_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_CDC_COUNT", num_interfaces)
    add_idf_sdkconfig_option(
        "CONFIG_TINYUSB_CDC_RX_BUFSIZE", config[CONF_USB_RX_BUFFER_SIZE]
    )
    add_idf_sdkconfig_option(
        "CONFIG_TINYUSB_CDC_TX_BUFSIZE", config[CONF_USB_TX_BUFFER_SIZE]
    )
