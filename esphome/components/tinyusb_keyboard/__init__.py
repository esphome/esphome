from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_KEY

CONF_MODIFIERS = "modifiers"
CONF_USAGE = "usage"

tinyusb_keyboard_ns = cg.esphome_ns.namespace("tinyusb_keyboard")
TinyUSBKeyboard = tinyusb_keyboard_ns.class_("TinyUSBKeyboard", cg.Component)

PressAction = tinyusb_keyboard_ns.class_("PressAction", automation.Action)
ReleaseAction = tinyusb_keyboard_ns.class_("ReleaseAction", automation.Action)

MediaPressAction = tinyusb_keyboard_ns.class_("MediaPressAction", automation.Action)
MediaReleaseAction = tinyusb_keyboard_ns.class_("MediaReleaseAction", automation.Action)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TinyUSBKeyboard),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    # Ensure TinyUSB HID support is enabled in IDF sdkconfig so HID APIs/headers are available
    add_idf_sdkconfig_option("CONFIG_TINYUSB_HID_COUNT", 1)
    cg.add_define("TINYUSB_KEYBOARD")


PRESS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required(CONF_KEY): cv.Any(cv.string, cv.positive_int),
        cv.Optional(CONF_MODIFIERS, default=0): cv.positive_int,
    }
)

RELEASE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
    }
)

MEDIA_PRESS_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required(CONF_USAGE): cv.positive_int,
    }
)

MEDIA_RELEASE_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
    }
)


@automation.register_action(
    "tinyusb_keyboard.press", PressAction, PRESS_ACTION_SCHEMA, synchronous=True
)
async def tinyusb_keyboard_press_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    key = config[CONF_KEY]
    if isinstance(key, str) and len(key) == 1:
        key_template = await cg.templatable(key, args, cg.std_string)
        cg.add(var.set_key(key_template))
    else:
        key_template = await cg.templatable(key, args, cg.uint8)
        cg.add(var.set_key_code(key_template))

    modifiers = config.get(CONF_MODIFIERS, 0)
    mod_template = await cg.templatable(modifiers, args, cg.uint8)
    cg.add(var.set_modifiers(mod_template))

    return var


@automation.register_action(
    "tinyusb_keyboard.release", ReleaseAction, RELEASE_ACTION_SCHEMA, synchronous=True
)
async def tinyusb_keyboard_release_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "tinyusb_keyboard.media_press",
    MediaPressAction,
    MEDIA_PRESS_SCHEMA,
    synchronous=True,
)
async def tinyusb_keyboard_media_press_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    template_ = await cg.templatable(config[CONF_USAGE], args, cg.uint16)
    cg.add(var.set_usage(template_))
    return var


@automation.register_action(
    "tinyusb_keyboard.media_release",
    MediaReleaseAction,
    MEDIA_RELEASE_SCHEMA,
    synchronous=True,
)
async def tinyusb_keyboard_media_release_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)
