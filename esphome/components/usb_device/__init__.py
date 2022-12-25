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
    cg.add_library(
        "https://github.com/adafruit/Adafruit_TinyUSB_Arduino/archive/1967830558d277dc211c1ca3a042c7dfa12b7466.zip",
        None,
    )
    cg.add_build_flag("-DCFG_TUSB_MCU=OPT_MCU_" + CORE.data[KEY_ESP32][KEY_VARIANT])
    cg.add_build_flag("-DCFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE")
    cg.add_build_flag("-DCFG_TUSB_OS=OPT_OS_FREERTOS")
    cg.add_platformio_option("build_unflags", "-DARDUINO_USB_CDC_ON_BOOT=1")
