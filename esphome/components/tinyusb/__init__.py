import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    add_idf_sdkconfig_option,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@kbx81"]
CONFLICTS_WITH = ["usb_host"]

CONF_USB_LANG_ID = "usb_lang_id"
CONF_USB_MANUFACTURER_STR = "usb_manufacturer_str"
CONF_USB_PRODUCT_ID = "usb_product_id"
CONF_USB_PRODUCT_STR = "usb_product_str"
CONF_USB_SERIAL_STR = "usb_serial_str"
CONF_USB_VENDOR_ID = "usb_vendor_id"

tinyusb_ns = cg.esphome_ns.namespace("tinyusb")
TinyUSB = tinyusb_ns.class_("TinyUSB", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TinyUSB),
            cv.Optional(CONF_USB_PRODUCT_ID, default=0x4001): cv.uint16_t,
            cv.Optional(CONF_USB_VENDOR_ID, default=0x303A): cv.uint16_t,
            cv.Optional(CONF_USB_LANG_ID, default=0x0409): cv.uint16_t,
            cv.Optional(CONF_USB_MANUFACTURER_STR, default="ESPHome"): cv.string,
            cv.Optional(CONF_USB_PRODUCT_STR, default="ESPHome"): cv.string,
            cv.Optional(CONF_USB_SERIAL_STR, default=""): cv.string,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[VARIANT_ESP32P4, VARIANT_ESP32S2, VARIANT_ESP32S3],
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Set USB device descriptor properties
    cg.add(var.set_usb_desc_product_id(config[CONF_USB_PRODUCT_ID]))
    cg.add(var.set_usb_desc_vendor_id(config[CONF_USB_VENDOR_ID]))
    cg.add(var.set_usb_desc_lang_id(config[CONF_USB_LANG_ID]))
    cg.add(var.set_usb_desc_manufacturer(config[CONF_USB_MANUFACTURER_STR]))
    cg.add(var.set_usb_desc_product(config[CONF_USB_PRODUCT_STR]))
    if config[CONF_USB_SERIAL_STR]:
        cg.add(var.set_usb_desc_serial(config[CONF_USB_SERIAL_STR]))

    add_idf_component(name="espressif/esp_tinyusb", ref="1.7.6~1")

    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_DEFAULT_PID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_BCD_DEVICE", 0x0100)
