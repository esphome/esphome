import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import keyboard
from esphome.cpp_generator import MockObj
from .. import hid_has_keyboard_or_media_keys, HIDDevice, hid_ns

CODEOWNERS = ["@tomaszduda23"]

MediaKeys = hid_ns.class_("MediaKeys")
Keyboard = hid_ns.class_("Keyboard")


CONF_HID_ID = "hid_id"
CONF_KEYBOARD_CONTROL = "keyboard_control"
CONF_MEDIA_KEYS_CONTROL = "media_keys_control"

CONFIG_SCHEMA = cv.All(
    keyboard.KEYBOARD_SCHEMA.extend(
        {
            cv.Required(CONF_HID_ID): cv.use_id(HIDDevice),
            cv.GenerateID(CONF_KEYBOARD_CONTROL): cv.declare_id(Keyboard),
            cv.GenerateID(CONF_MEDIA_KEYS_CONTROL): cv.declare_id(MediaKeys),
        }
    ).extend(cv.COMPONENT_SCHEMA),
)


def check_hid_device(value):
    if not hid_has_keyboard_or_media_keys(value):
        raise cv.Invalid("HID device should has keyboard or media keys.")


FINAL_VALIDATE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_HID_ID): check_hid_device,
    },
    extra=cv.ALLOW_EXTRA,
)


async def to_code(config):
    hid = await cg.get_variable(config[CONF_HID_ID])
    keyboard_control = cg.Pvariable(
        config[CONF_KEYBOARD_CONTROL], hid.keyboard_control()
    )
    media_keys_control = cg.Pvariable(
        config[CONF_MEDIA_KEYS_CONTROL], hid.media_keys_control()
    )
    var = cg.new_Pvariable(config[CONF_ID], keyboard_control, media_keys_control)
    cg.add(MockObj("hid::Keyboard::set_parrent")(keyboard_control, var))
    await cg.register_component(var, config)
    await keyboard.register_keyboard(var, config)
