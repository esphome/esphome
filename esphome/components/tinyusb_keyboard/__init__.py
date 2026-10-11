import string

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_sdkconfig_option,
)
from esphome.components.tinyusb import TinyUSB
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY
from esphome.types import ConfigType

CODEOWNERS = ["@frederikbolding"]
# The keyboard supplies the whole configuration descriptor, with no room for the CDC interfaces
CONFLICTS_WITH = ["usb_cdc_acm"]
DEPENDENCIES = ["tinyusb"]
DOMAIN = "tinyusb_keyboard"

CONF_MODIFIERS = "modifiers"
CONF_TINYUSB_ID = "tinyusb_id"
CONF_USAGE = "usage"

# Keyboard page usage ids run without gaps from HID_KEY_A (0x04) through a..z, 1..9 and 0
HID_KEY_A = 0x04
KEY_CODES = {
    char: HID_KEY_A + index
    for index, char in enumerate(string.ascii_lowercase + "1234567890")
}

tinyusb_keyboard_ns = cg.esphome_ns.namespace("tinyusb_keyboard")
TinyUSBKeyboard = tinyusb_keyboard_ns.class_("TinyUSBKeyboard", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TinyUSBKeyboard),
            cv.GenerateID(CONF_TINYUSB_ID): cv.use_id(TinyUSB),
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

# A keyboard usage id as a number, or a single lowercase letter or digit
key_code = cv.Any(cv.enum(KEY_CODES), cv.uint8_t)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    tinyusb = await cg.get_variable(config[CONF_TINYUSB_ID])
    cg.add(tinyusb.set_full_speed_config(tinyusb_keyboard_ns.CONFIGURATION_DESCRIPTOR))
    add_idf_sdkconfig_option("CONFIG_TINYUSB_HID_COUNT", 1)


KEYBOARD_ID_SCHEMA = {cv.GenerateID(): cv.use_id(TinyUSBKeyboard)}

automation.register_apply_action(
    "tinyusb_keyboard.press",
    automation.maybe_simple_id(
        {
            **KEYBOARD_ID_SCHEMA,
            cv.Required(CONF_KEY): cv.templatable(key_code),
            cv.Optional(CONF_MODIFIERS, default=0): cv.templatable(cv.uint8_t),
        }
    ),
    automation.ApplyCall(
        "press_key({}, {})", ((CONF_KEY, cg.uint8), (CONF_MODIFIERS, cg.uint8))
    ),
)
automation.register_apply_action(
    "tinyusb_keyboard.release",
    automation.maybe_simple_id(KEYBOARD_ID_SCHEMA),
    automation.ApplyCall("release_keys()"),
)
automation.register_apply_action(
    "tinyusb_keyboard.media_press",
    automation.maybe_simple_id(
        {**KEYBOARD_ID_SCHEMA, cv.Required(CONF_USAGE): cv.templatable(cv.uint16_t)}
    ),
    automation.ApplyCall("press_media({})", ((CONF_USAGE, cg.uint16),)),
)
automation.register_apply_action(
    "tinyusb_keyboard.media_release",
    automation.maybe_simple_id(KEYBOARD_ID_SCHEMA),
    automation.ApplyCall("release_media()"),
)
