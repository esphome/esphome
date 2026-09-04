from __future__ import annotations

from esphome import automation
import esphome.codegen as cg
from esphome.components.zephyr import zephyr_add_prj_conf
import esphome.config_validation as cv
from esphome.const import CONF_ID, KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE

CONF_KEYCODE = "keycode"

CODEOWNERS = ["@tomaszduda23"]

usb_hid_keyboard_ns = cg.esphome_ns.namespace("usb_hid_keyboard")
USBHIDKeyboard = usb_hid_keyboard_ns.class_("USBHIDKeyboard", cg.Component)
KeyPressAction = usb_hid_keyboard_ns.class_("KeyPressAction", automation.Action)
KeyReleaseAction = usb_hid_keyboard_ns.class_("KeyReleaseAction", automation.Action)

# Standard USB HID keyboard usage IDs (USB HID Usage Tables 1.12, Section 10)
KEY_CODES: dict[str, int] = {
    "NONE": 0x00,
    # Letters
    "A": 0x04,
    "B": 0x05,
    "C": 0x06,
    "D": 0x07,
    "E": 0x08,
    "F": 0x09,
    "G": 0x0A,
    "H": 0x0B,
    "I": 0x0C,
    "J": 0x0D,
    "K": 0x0E,
    "L": 0x0F,
    "M": 0x10,
    "N": 0x11,
    "O": 0x12,
    "P": 0x13,
    "Q": 0x14,
    "R": 0x15,
    "S": 0x16,
    "T": 0x17,
    "U": 0x18,
    "V": 0x19,
    "W": 0x1A,
    "X": 0x1B,
    "Y": 0x1C,
    "Z": 0x1D,
    # Digits
    "1": 0x1E,
    "2": 0x1F,
    "3": 0x20,
    "4": 0x21,
    "5": 0x22,
    "6": 0x23,
    "7": 0x24,
    "8": 0x25,
    "9": 0x26,
    "0": 0x27,
    # Control keys
    "ENTER": 0x28,
    "ESCAPE": 0x29,
    "BACKSPACE": 0x2A,
    "TAB": 0x2B,
    "SPACE": 0x2C,
    "MINUS": 0x2D,
    "EQUAL": 0x2E,
    "LEFT_BRACKET": 0x2F,
    "RIGHT_BRACKET": 0x30,
    "BACKSLASH": 0x31,
    "SEMICOLON": 0x33,
    "APOSTROPHE": 0x34,
    "GRAVE": 0x35,
    "COMMA": 0x36,
    "PERIOD": 0x37,
    "SLASH": 0x38,
    "CAPS_LOCK": 0x39,
    # Function keys
    "F1": 0x3A,
    "F2": 0x3B,
    "F3": 0x3C,
    "F4": 0x3D,
    "F5": 0x3E,
    "F6": 0x3F,
    "F7": 0x40,
    "F8": 0x41,
    "F9": 0x42,
    "F10": 0x43,
    "F11": 0x44,
    "F12": 0x45,
    # Navigation
    "PRINT_SCREEN": 0x46,
    "SCROLL_LOCK": 0x47,
    "PAUSE": 0x48,
    "INSERT": 0x49,
    "HOME": 0x4A,
    "PAGE_UP": 0x4B,
    "DELETE": 0x4C,
    "END": 0x4D,
    "PAGE_DOWN": 0x4E,
    "ARROW_RIGHT": 0x4F,
    "ARROW_LEFT": 0x50,
    "ARROW_DOWN": 0x51,
    "ARROW_UP": 0x52,
    # Keypad
    "NUM_LOCK": 0x53,
    "KP_DIVIDE": 0x54,
    "KP_MULTIPLY": 0x55,
    "KP_MINUS": 0x56,
    "KP_PLUS": 0x57,
    "KP_ENTER": 0x58,
    "KP_1": 0x59,
    "KP_2": 0x5A,
    "KP_3": 0x5B,
    "KP_4": 0x5C,
    "KP_5": 0x5D,
    "KP_6": 0x5E,
    "KP_7": 0x5F,
    "KP_8": 0x60,
    "KP_9": 0x61,
    "KP_0": 0x62,
    "KP_PERIOD": 0x63,
    # Media / consumer keys (via keyboard page for simple use)
    "MUTE": 0x7F,
    "VOLUME_UP": 0x80,
    "VOLUME_DOWN": 0x81,
}

# Modifier bitmask values (byte 0 of the 8-byte keyboard report)
MODIFIER_CODES: dict[str, int] = {
    "NONE": 0x00,
    "LEFT_CTRL": 0x01,
    "LEFT_SHIFT": 0x02,
    "LEFT_ALT": 0x04,
    "LEFT_GUI": 0x08,
    "RIGHT_CTRL": 0x10,
    "RIGHT_SHIFT": 0x20,
    "RIGHT_ALT": 0x40,
    "RIGHT_GUI": 0x80,
}

CONF_MODIFIER = "modifier"


def _validate_key(value):
    if isinstance(value, int):
        return cv.uint8_t(value)
    if isinstance(value, str):
        upper = value.upper()
        if upper in KEY_CODES:
            return KEY_CODES[upper]
        # Allow raw numeric string (e.g. "0x52" or "82")
        try:
            return cv.uint8_t(int(value, 0))
        except (ValueError, TypeError):
            pass
    raise cv.Invalid(
        f"Unknown key '{value}'. Use a named key (e.g. ARROW_UP, ENTER) "
        f"or a raw code (e.g. 0x52). Valid names: {', '.join(sorted(KEY_CODES))}"
    )


def _validate_modifier_single(value):
    if isinstance(value, int):
        return cv.uint8_t(value)
    if isinstance(value, str):
        upper = value.upper()
        if upper in MODIFIER_CODES:
            return MODIFIER_CODES[upper]
        try:
            return cv.uint8_t(int(value, 0))
        except (ValueError, TypeError):
            pass
    raise cv.Invalid(
        f"Unknown modifier '{value}'. Valid modifiers: {', '.join(sorted(MODIFIER_CODES))}"
    )


def _validate_modifiers(value):
    if isinstance(value, list):
        result = 0
        for item in value:
            result |= _validate_modifier_single(item)
        return result
    return _validate_modifier_single(value)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(USBHIDKeyboard),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_framework("zephyr"),
)

KEY_PRESS_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(USBHIDKeyboard),
        cv.Required(CONF_KEYCODE): _validate_key,
        cv.Optional(CONF_MODIFIER, default=0): _validate_modifiers,
    }
)

KEY_RELEASE_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(CONF_ID): cv.use_id(USBHIDKeyboard),
    }
)


@automation.register_action(
    "usb_hid_keyboard.press", KeyPressAction, KEY_PRESS_SCHEMA, synchronous=True
)
async def key_press_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    cg.add(var.set_keycode(config[CONF_KEYCODE]))
    cg.add(var.set_modifier(config[CONF_MODIFIER]))
    return var


@automation.register_action(
    "usb_hid_keyboard.release", KeyReleaseAction, KEY_RELEASE_SCHEMA, synchronous=True
)
async def key_release_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Disable the new USB device stack next if present (sdk-nrf >= 3.2.0 enables it
    # by default, but the old stack API is what we use here)
    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]
    if CORE.is_nrf52 and framework_ver >= cv.Version(3, 2, 0):
        zephyr_add_prj_conf("USB_DEVICE_STACK_NEXT", False)

    zephyr_add_prj_conf("USB_DEVICE_STACK", True)
    zephyr_add_prj_conf("USB_DEVICE_HID", True)
    # Reserve one HID device slot (the default is 1; set explicitly for clarity)
    zephyr_add_prj_conf("USB_HID_DEVICE_COUNT", 1, required=False)
    # 8-byte interrupt endpoint matches boot-keyboard report size
    zephyr_add_prj_conf("HID_INTERRUPT_EP_MPS", 8)
    # Prevent USB suspend from stopping communication
    zephyr_add_prj_conf("USB_DEVICE_REMOTE_WAKEUP", False)
