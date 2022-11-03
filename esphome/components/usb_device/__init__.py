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
)
import esphome.final_validate as fv
from esphome.components.hid import CONF_HID

CODEOWNERS = ["@tomaszduda23"]
CONF_USB_DEVICE_ID = "usb_device_id"


def _validate_variant(value):
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in [VARIANT_ESP32S2]:
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

# adafruit/Adafruit TinyUSB Library 1.14.4 use singleton for HID
# the limit should be number of EPs (excluding EPs which has special purpose in esp-idf e.g. CDC)
CONF_MAX_HID_NUMBER = 1


def final_validate_number_of_device(config):
    fconf = fv.full_config.get()
    if CONF_HID in fconf:
        count_hid = sum(
            map(lambda d: d.get("platform") == CONF_USB_DEVICE, fconf[CONF_HID])
        )
        if count_hid > CONF_MAX_HID_NUMBER:
            raise cv.Invalid(f"Maximum number of HID devices is {CONF_MAX_HID_NUMBER}")
    else:
        raise cv.Invalid("Please define at least one USB device")


FINAL_VALIDATE_SCHEMA = cv.All(
    final_validate_number_of_device,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add_library("adafruit/Adafruit TinyUSB Library", "1.14.4", None)
    cg.add_build_flag("-DCFG_TUSB_MCU=OPT_MCU_ESP32S2")
    cg.add_build_flag("-DCFG_TUSB_RHPORT0_MODE=OPT_MODE_DEVICE")
    cg.add_build_flag("-DCFG_TUSB_OS=OPT_OS_FREERTOS")
