import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.core import CORE
from esphome.const import (
    CONF_ID,
)
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)

CODEOWNERS = ["@tomaszduda23"]
CONF_USB_DEVICE_ID = "usb_device_id"


def _validate_variant(value):
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in [VARIANT_ESP32S2, VARIANT_ESP32S3]:
        raise cv.Invalid(f"USB device is unsupported by ESP32 variant {variant}")
    return value


CONF_USB_DEVICE = "usb_device"

usb_device_ns = cg.esphome_ns.namespace(CONF_USB_DEVICE)
UsbDevice = usb_device_ns.class_("UsbDevice", cg.PollingComponent)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(UsbDevice),
        }
    ).extend(cv.polling_component_schema("10s")),
    cv.only_with_arduino,
    cv.only_on_esp32,
    _validate_variant,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_build_flag("-Wl,--wrap=tud_mount_cb")
    cg.add_build_flag("-Wl,--wrap=tud_umount_cb")
