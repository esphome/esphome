import dataclasses
import csv
import os
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE
from esphome.core import CORE
from esphome import automation
from esphome.cpp_helpers import setup_entity
from esphome.cpp_generator import MockObj

IS_PLATFORM_COMPONENT = True
CODEOWNERS = ["@tomaszduda23"]


@dataclasses.dataclass(frozen=True)
class _UsbKey:
    code: int
    is_modifier: bool
    is_media_key: bool

    @property
    def arduino_modifier_code(self) -> int:
        # https://github.com/NicoHood/HID/blob/4bf6cd6/src/HID-APIs/DefaultKeyboardAPI.hpp#L31
        assert self.is_modifier
        code = self.code
        offset = 0
        while not (code & 0x1):
            code >>= 1
            offset += 1
            assert offset < 8
        return (0xE << 4) | offset


@dataclasses.dataclass(frozen=True)
class _Ps2Key:
    code: int
    type: str


@dataclasses.dataclass(frozen=True)
class _KeyMapping:
    web_name: str
    mcu_code: int
    usb_key: _UsbKey
    ps2_key: _Ps2Key
    at1_code: int
    hid_code: int


def _parse_usb_key(key: str) -> _UsbKey:
    is_modifier = key.startswith("^")
    is_media_key = key.startswith("@")
    code = int((key[1:] if is_modifier or is_media_key else key), 16)
    return _UsbKey(code, is_modifier, is_media_key)


def _parse_ps2_key(key: str) -> _Ps2Key:
    (code_type, raw_code) = key.split(":")
    return _Ps2Key(
        code=int(raw_code, 16),
        type=code_type,
    )


def _read_keymap_csv(path: str) -> list[_KeyMapping]:
    _keymap: list[_KeyMapping] = []
    with open(path, encoding="utf-8") as keymap_file:
        for row in csv.DictReader(keymap_file):
            if len(row) >= 6:
                usb_key = _parse_usb_key(row["usb_key"])
                if usb_key.is_modifier:
                    hid_code = usb_key.arduino_modifier_code
                else:
                    hid_code = usb_key.code
                _keymap.append(
                    _KeyMapping(
                        web_name=row["web_name"],
                        mcu_code=int(row["mcu_code"]),
                        usb_key=usb_key,
                        ps2_key=_parse_ps2_key(row["ps2_key"]),
                        at1_code=int(row["at1_code"], 16),
                        hid_code=hid_code,
                        # x11_keys=_parse_x11_names(row["x11_names"] or ""),
                    )
                )
    return _keymap


# https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/code/code_values
# https://github.com/NicoHood/HID/blob/master/src/KeyboardLayouts/ImprovedKeylayouts.h
# https://github.com/Harvie/ps2dev/blob/master/src/ps2dev.h
# https://gist.github.com/MightyPork/6da26e382a7ad91b5496ee55fdc73db2
# https://github.com/qemu/keycodemapdb/blob/master/data/keymaps.csv
# Hut1_12v2.pdf
# Fields list:
#   - Web
#   - MCU code
#   - USB code (^ for the modifier mask)
#   - PS/2 key
#   - AT set1
#   - X11 keysyms (^ for shift)


def _gen_keymap(_keymap):
    _key_map = {}
    for key in _keymap:
        _key_map[key.web_name] = key.hid_code
    return _key_map


base_dir = os.path.dirname(os.path.realpath(__file__))
keymap = _read_keymap_csv(os.path.join(base_dir, "keymap.csv"))

CONF_KEYS = "keys"
CONF_KEYS_MAP = _gen_keymap(keymap)

keyboard_ns = cg.esphome_ns.namespace("keyboard")
Keyboard = keyboard_ns.class_("Keyboard", cg.Component)


KEYBOARD_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Keyboard),
        }
    )
).extend(cv.COMPONENT_SCHEMA)


async def setup_keyboard_core_(var, config):
    await setup_entity(var, config)


async def register_keyboard(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    cg.add(cg.App.register_keyboard(var))
    await setup_keyboard_core_(var, config)


def validate_keys(value):
    if isinstance(value, str):
        if CONF_KEYS_MAP.get(value) is None:
            raise cv.Invalid("Not mapping for " + value)
        return CONF_KEYS_MAP[value]
    if isinstance(value, int):
        cv.int_range(min=1, max=65535)(value)
        return value
    raise cv.Invalid("keys must either be a string wrapped in quotes or a int")


CONF_KEYBOARD = "keyboard"
CONF_MEDIA_KEYS = "media_keys"

CONF_KEYBOARD_TYPE = {
    CONF_KEYBOARD: "KEYBOARD",
    CONF_MEDIA_KEYS: "MEDIA_KEYS",
}


def add_key_action(name, action_name, is_required):
    Action = keyboard_ns.class_(name, automation.Action)

    @automation.register_action(
        action_name,
        Action,
        cv.Schema(
            {
                cv.GenerateID(): cv.use_id(Keyboard),
                cv.Required(CONF_KEYS)
                if is_required
                else cv.Optional(CONF_KEYS): cv.ensure_list(validate_keys),
                cv.Optional(CONF_TYPE, CONF_KEYBOARD): cv.All(
                    cv.enum(CONF_KEYBOARD_TYPE, lower=True),
                    cv.Length(min=1),
                ),
            }
        ),
    )
    async def keyboard_send_key_to_code(config, action_id, template_arg, args):
        var = cg.new_Pvariable(action_id, template_arg)
        await cg.register_parented(var, config[CONF_ID])
        if CONF_KEYS in config:
            key = config[CONF_KEYS]
            cg.add(var.set_key(key))
        cg.add(var.set_type(MockObj(CONF_KEYBOARD_TYPE[config[CONF_TYPE]])))
        return var


add_key_action("DownAction", "keyboard.down", True)
add_key_action("UpAction", "keyboard.up", True)
add_key_action("SetAction", "keyboard.set", False)


async def to_code(config):
    cg.add_global(keyboard_ns.using)
    cg.add_define("USE_KEYBOARD")
