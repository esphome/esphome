import esphome.final_validate as fv
import esphome.codegen as cg
from esphome.components.keyboard import CONF_KEYBOARD, CONF_MEDIA_KEYS
from esphome.const import CONF_TYPE

IS_PLATFORM_COMPONENT = True

CONF_COMPOSITE = "composite"
CONF_BOOT_PROTOCOL = "boot_protocol"
CONF_HID = "hid"

hid_ns = cg.esphome_ns.namespace(CONF_HID)
HIDDevice = hid_ns.class_("HIDDevice", cg.Component)


def has_keyboard(config):
    if CONF_TYPE not in config:
        return False
    if CONF_COMPOSITE in config[CONF_TYPE]:
        return bool(CONF_KEYBOARD in config[CONF_TYPE][CONF_COMPOSITE])
    if (
        CONF_BOOT_PROTOCOL in config[CONF_TYPE]
        and CONF_KEYBOARD == config[CONF_TYPE][CONF_BOOT_PROTOCOL]
    ):
        return True
    return False


def has_media_keys(config):
    if CONF_TYPE not in config:
        return False
    if (
        CONF_COMPOSITE in config[CONF_TYPE]
        and CONF_MEDIA_KEYS in config[CONF_TYPE][CONF_COMPOSITE]
    ):
        return True
    return False


def hid_has_keyboard_or_media_keys(value):
    fconf = fv.full_config.get()
    hid_path = fconf.get_path_for_id(value)[:2]
    hid = fconf.get_config_for_path(hid_path)
    if has_keyboard(hid) or has_media_keys(hid):
        return True
    return False
