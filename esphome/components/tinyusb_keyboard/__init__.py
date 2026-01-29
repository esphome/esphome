from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

tinyusb_keyboard_ns = cg.esphome_ns.namespace("tinyusb_keyboard")
TinyUSBKeyboard = tinyusb_keyboard_ns.class_("TinyUSBKeyboard", cg.Component)

# Action classes
PressAction = tinyusb_keyboard_ns.class_("PressAction", automation.Action)
ReleaseAction = tinyusb_keyboard_ns.class_("ReleaseAction", automation.Action)

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


# Action schemas
PRESS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required("key"): cv.Any(cv.string, cv.positive_int),
        cv.Optional("modifiers", default=0): cv.positive_int,
    }
)

RELEASE_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required("key"): cv.Any(cv.string, cv.positive_int),
    }
)


@automation.register_action("tinyusb_keyboard.press", PressAction, PRESS_ACTION_SCHEMA)
async def tinyusb_keyboard_press_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    # Accept single-character strings or numeric HID keycodes
    key = config["key"]
    if isinstance(key, str) and len(key) == 1:
        # single char -> pass as char template
        key_template = await cg.templatable(key, args, str)
        cg.add(var.set_key(key_template))
    else:
        # numeric
        key_template = await cg.templatable(key, args, int)
        cg.add(var.set_key_code(key_template))

    modifiers = config.get("modifiers", 0)
    mod_template = await cg.templatable(modifiers, args, int)
    cg.add(var.set_modifiers(mod_template))

    return var


@automation.register_action(
    "tinyusb_keyboard.release", ReleaseAction, RELEASE_ACTION_SCHEMA
)
async def tinyusb_keyboard_release_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    key = config["key"]
    if isinstance(key, str) and len(key) == 1:
        key_template = await cg.templatable(key, args, str)
        cg.add(var.set_key(key_template))
    else:
        key_template = await cg.templatable(key, args, int)
        cg.add(var.set_key_code(key_template))

    return var


# Media (consumer) actions
MediaPressAction = tinyusb_keyboard_ns.class_("MediaPressAction", automation.Action)
MediaReleaseAction = tinyusb_keyboard_ns.class_("MediaReleaseAction", automation.Action)

MEDIA_PRESS_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required("usage"): cv.positive_int,
    }
)

MEDIA_RELEASE_SCHEMA = maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(TinyUSBKeyboard),
        cv.Required("usage"): cv.positive_int,
    }
)


@automation.register_action(
    "tinyusb_keyboard.media_press", MediaPressAction, MEDIA_PRESS_SCHEMA
)
async def tinyusb_keyboard_media_press_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    usage = config["usage"]
    template_ = await cg.templatable(usage, args, int)
    cg.add(var.set_usage(template_))
    return var


@automation.register_action(
    "tinyusb_keyboard.media_release", MediaReleaseAction, MEDIA_RELEASE_SCHEMA
)
async def tinyusb_keyboard_media_release_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)

    usage = config["usage"]
    template_ = await cg.templatable(usage, args, int)
    cg.add(var.set_usage(template_))
    return var
