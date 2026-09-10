from esphome import automation, final_validate as fv, pins
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_component,
    add_idf_sdkconfig_option,
)
import esphome.config_validation as cv
from esphome.const import CONF_HARDWARE_UART, CONF_ID
from esphome.types import ConfigType

CODEOWNERS = ["@kbx81"]
CONFLICTS_WITH = ["usb_host"]

CONF_ON_MOUNT = "on_mount"
CONF_ON_UNMOUNT = "on_unmount"
CONF_USB_LANG_ID = "usb_lang_id"
CONF_USB_MANUFACTURER_STR = "usb_manufacturer_str"
CONF_USB_PRODUCT_ID = "usb_product_id"
CONF_USB_PRODUCT_STR = "usb_product_str"
CONF_USB_SERIAL_STR = "usb_serial_str"
CONF_USB_VENDOR_ID = "usb_vendor_id"
CONF_VBUS_MONITOR_PIN = "vbus_monitor_pin"

# Components that provide a USB device class (CDC, HID, MSC, ...) on top of
# tinyusb. Configuring `tinyusb:` without any of these triggers a 5s hang in
# esp_tinyusb's driver install (descriptors_set fails with no class and no
# user-provided full_speed_config), which trips the task watchdog before
# loop() ever runs.
_USB_CLASS_COMPONENTS = ("usb_cdc_acm",)

tinyusb_ns = cg.esphome_ns.namespace("tinyusb")
TinyUSB = tinyusb_ns.class_("TinyUSB", cg.Component)
IsMountedCondition = tinyusb_ns.class_("IsMountedCondition", automation.Condition)

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
            cv.Optional(CONF_VBUS_MONITOR_PIN): pins.internal_gpio_input_pin_number,
            cv.Optional(CONF_ON_MOUNT): automation.validate_automation({}),
            cv.Optional(CONF_ON_UNMOUNT): automation.validate_automation({}),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    esp32.only_on_variant(
        supported=[
            VARIANT_ESP32H4,
            VARIANT_ESP32P4,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32S31,
        ],
    ),
)


def _validate_vbus_monitor_pin(config: ConfigType) -> ConfigType:
    # esp_tinyusb monitors VBUS on the S31 through a GPIO interrupt and needs the GPIO
    # ISR service installed first, which would collide with the esp32 platform's own
    # lazy install and disable other interrupts. The other variants watch the pin in
    # the OTG hardware.
    if (
        CONF_VBUS_MONITOR_PIN in config
        and esp32.get_esp32_variant() == VARIANT_ESP32S31
    ):
        raise cv.Invalid(
            f"{CONF_VBUS_MONITOR_PIN} is not supported on the ESP32-S31 yet",
            [CONF_VBUS_MONITOR_PIN],
        )
    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate_vbus_monitor_pin)


def _final_validate(config: ConfigType) -> None:
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
            "USB_SERIAL_JTAG on variants that support it "
            "(ESP32-S3, ESP32-S31, ESP32-P4, ESP32-H4)"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
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
    if (vbus_pin := config.get(CONF_VBUS_MONITOR_PIN)) is not None:
        cg.add(var.set_vbus_monitor_pin(vbus_pin))

    for conf_key, forwarder in (
        (CONF_ON_MOUNT, automation.TriggerOnTrueForwarder),
        (CONF_ON_UNMOUNT, automation.TriggerOnFalseForwarder),
    ):
        for conf in config.get(conf_key, []):
            await automation.build_callback_automation(
                var, "add_on_mount_state_callback", [], conf, forwarder=forwarder
            )

    add_idf_component(name="espressif/esp_tinyusb", ref="2.2.1")

    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_ESPRESSIF_VID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_USE_DEFAULT_PID", False)
    add_idf_sdkconfig_option("CONFIG_TINYUSB_DESC_BCD_DEVICE", 0x0100)


@automation.register_condition(
    "tinyusb.is_mounted",
    IsMountedCondition,
    cv.Schema({cv.GenerateID(): cv.use_id(TinyUSB)}),
)
async def tinyusb_is_mounted_to_code(config, condition_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(condition_id, template_arg, paren)
