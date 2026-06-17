from esphome import final_validate as fv
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
from esphome.const import CONF_HARDWARE_UART, CONF_ID

CODEOWNERS = ["@kbx81"]
CONFLICTS_WITH = ["usb_host"]

CONF_USB_LANG_ID = "usb_lang_id"
CONF_USB_MANUFACTURER_STR = "usb_manufacturer_str"
CONF_USB_PRODUCT_ID = "usb_product_id"
CONF_USB_PRODUCT_STR = "usb_product_str"
CONF_USB_SERIAL_STR = "usb_serial_str"
CONF_USB_VENDOR_ID = "usb_vendor_id"

# Components that provide a USB device class (CDC, HID, MSC, ...) on top of
# tinyusb. Configuring `tinyusb:` without any of these triggers a 5s hang in
# esp_tinyusb's driver install (descriptors_set fails with no class and no
# user-provided full_speed_config), which trips the task watchdog before
# loop() ever runs.
_USB_CLASS_COMPONENTS = ("usb_cdc_acm",)

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


def _final_validate(config):
    full_config = fv.full_config.get()
    if not any(name in full_config for name in _USB_CLASS_COMPONENTS):
        raise cv.Invalid(
            "The 'tinyusb' component requires at least one USB class component"
        )
    # tinyusb owns the USB OTG peripheral. The logger's USB_CDC backend routes
    # the ROM console through that same peripheral, so the two cannot coexist.
    # (USB_SERIAL_JTAG is a separate peripheral and is fine alongside tinyusb.)
    logger_config = full_config.get("logger")
    if logger_config and logger_config.get(CONF_HARDWARE_UART) == "USB_CDC":
        raise cv.Invalid(
            "'tinyusb' cannot be used with 'logger.hardware_uart: USB_CDC' "
            "because both share the USB OTG peripheral. Set "
            "'logger.hardware_uart' to a hardware UART (e.g. UART0), or to "
            "USB_SERIAL_JTAG on variants that support it (ESP32-S3, ESP32-P4)"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


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

    add_idf_component(name="espressif/esp_tinyusb", ref="2.1.1")

    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_DEFAULT_PID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_BCD_DEVICE", 0x0100)
