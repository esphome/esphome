import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from esphome.cpp_generator import MockObj
from esphome.components import hid
from esphome.components.hid import (
    CONF_KEYBOARD,
    CONF_MEDIA_KEYS,
    CONF_BOOT_PROTOCOL,
    CONF_COMPOSITE,
)
from .. import usb_device_ns, CONF_USB_DEVICE

AUTO_LOAD = [CONF_USB_DEVICE]
CODEOWNERS = ["@tomaszduda23"]

USBHIDDevice = usb_device_ns.class_("USBHIDDevice", hid.HIDDevice)
KeyboardReport = usb_device_ns.class_("KeyboardReport", cg.Component)
MediaKeysReport = usb_device_ns.class_("MediaKeysReport", cg.Component)
Adafruit_USBD_HID = cg.global_ns.class_("Adafruit_USBD_HID", cg.Component)

CONF_MOUSE = "mouse"

BOOT_PROTOCOLS = {
    CONF_KEYBOARD: "HID_ITF_PROTOCOL_KEYBOARD",
    CONF_MOUSE: "HID_ITF_PROTOCOL_MOUSE",
}

CONF_COMPOSITE_DEVICES = {
    CONF_KEYBOARD: "TUD_HID_REPORT_DESC_KEYBOARD",
    CONF_MOUSE: "TUD_HID_REPORT_DESC_MOUSE",
    CONF_MEDIA_KEYS: "TUD_HID_REPORT_DESC_CONSUMER",
}

CONF_DESC_HID_REPORT_ID = "desc_hid_report_id"
CONF_USB_HID_ID = "usb_hid_id"
CONF_KEYBOARD_ID = "keyboard_id"
CONF_MEDIA_KEYS_ID = "media_keys_id"


def ensure_unique(value):
    all_values = list(value)
    unique_values = set(value)
    if len(all_values) != len(unique_values):
        raise cv.Invalid("Values must be unique.")
    return value


def is_keyboard_or_media(value):
    if value in [CONF_KEYBOARD, CONF_MEDIA_KEYS]:
        cv.requires_component(CONF_KEYBOARD)(value)
    return value


CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(USBHIDDevice),
        cv.Required(CONF_TYPE): {
            cv.Exclusive(CONF_BOOT_PROTOCOL, CONF_TYPE): cv.All(
                cv.enum(BOOT_PROTOCOLS, lower=True),
                is_keyboard_or_media,
            ),
            cv.Exclusive(CONF_COMPOSITE, CONF_TYPE): cv.All(
                cv.ensure_list(
                    cv.enum(CONF_COMPOSITE_DEVICES, lower=True), is_keyboard_or_media
                ),
                cv.Length(min=1),
                ensure_unique,
            ),
        },
        cv.GenerateID(CONF_DESC_HID_REPORT_ID): cv.declare_id(cg.uint8),
        cv.GenerateID(CONF_USB_HID_ID): cv.declare_id(Adafruit_USBD_HID),
        cv.GenerateID(CONF_KEYBOARD_ID): cv.declare_id(KeyboardReport),
        cv.GenerateID(CONF_MEDIA_KEYS_ID): cv.declare_id(MediaKeysReport),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    report_id = {}
    rhs = []
    if CONF_COMPOSITE in config[CONF_TYPE]:
        number = 1
        for device in config[CONF_TYPE][CONF_COMPOSITE]:
            report = (
                "HID_REPORT_ID(" + str(number) + ")"
                if len(config[CONF_TYPE][CONF_COMPOSITE]) > 1
                else ""
            )
            rhs += [MockObj(f"{CONF_COMPOSITE_DEVICES[device]}({report})")]
            report_id[device] = number
            number += 1
    else:
        if CONF_BOOT_PROTOCOL in config[CONF_TYPE]:
            device = config[CONF_TYPE][CONF_BOOT_PROTOCOL]
        rhs += [MockObj(f"{CONF_COMPOSITE_DEVICES[device]}()")]
        report_id[device] = 0

    desc = cg.progmem_array(config[CONF_DESC_HID_REPORT_ID], rhs)
    boot_protocol = "HID_ITF_PROTOCOL_NONE"
    if CONF_BOOT_PROTOCOL in config[CONF_TYPE]:
        boot_protocol = BOOT_PROTOCOLS[config[CONF_TYPE][CONF_BOOT_PROTOCOL]]
    usb_hid = cg.new_Pvariable(
        config[CONF_USB_HID_ID],
        desc,
        MockObj("sizeof")(desc),
        MockObj(boot_protocol),
        2,
        False,
    )
    var = cg.new_Pvariable(config[CONF_ID], usb_hid)
    await cg.register_component(var, config)
    if CONF_KEYBOARD in report_id:
        keyboard = cg.new_Pvariable(
            config[CONF_KEYBOARD_ID], usb_hid, report_id[CONF_KEYBOARD]
        )
        cg.add(var.set_report(keyboard))
    if CONF_MEDIA_KEYS in report_id:
        media_keys = cg.new_Pvariable(
            config[CONF_MEDIA_KEYS_ID], usb_hid, report_id[CONF_MEDIA_KEYS]
        )
        cg.add(var.set_report(media_keys))
    cg.add_build_flag("-DCONFIG_TINYUSB_HID_ENABLED=1")
    cg.add_build_flag("-DCFG_TUD_HID=CONFIG_TINYUSB_HID_ENABLED")
