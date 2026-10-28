"""X11 keysyms and pointer button masks for VNC input, resolved at code generation."""

import esphome.config_validation as cv
from esphome.core import HexInt

_KEYSYM_MAX = 0x1FFFFFFF
# X11 encodes a Unicode character outside Latin-1 as this prefix plus its code point
_UNICODE_KEYSYM = 0x01000000

KEYSYMS: dict[str, int] = {
    "BackSpace": 0xFF08,
    "Tab": 0xFF09,
    "Return": 0xFF0D,
    "Pause": 0xFF13,
    "Scroll_Lock": 0xFF14,
    "Escape": 0xFF1B,
    "Home": 0xFF50,
    "Left": 0xFF51,
    "Up": 0xFF52,
    "Right": 0xFF53,
    "Down": 0xFF54,
    "Page_Up": 0xFF55,
    "Page_Down": 0xFF56,
    "End": 0xFF57,
    "Print": 0xFF61,
    "Insert": 0xFF63,
    "Menu": 0xFF67,
    "Num_Lock": 0xFF7F,
    "KP_Enter": 0xFF8D,
    **{f"F{n}": 0xFFBE + n - 1 for n in range(1, 13)},
    "Shift_L": 0xFFE1,
    "Shift_R": 0xFFE2,
    "Control_L": 0xFFE3,
    "Control_R": 0xFFE4,
    "Caps_Lock": 0xFFE5,
    "Meta_L": 0xFFE7,
    "Meta_R": 0xFFE8,
    "Alt_L": 0xFFE9,
    "Alt_R": 0xFFEA,
    "Super_L": 0xFFEB,
    "Super_R": 0xFFEC,
    "Delete": 0xFFFF,
    "space": 0x20,
    "ISO_Left_Tab": 0xFE20,
}

ALIASES: dict[str, str] = {
    "alt": "Alt_L",
    "ctrl": "Control_L",
    "del": "Delete",
    "enter": "Return",
    "esc": "Escape",
    "ins": "Insert",
    "next": "Page_Down",
    "pgdn": "Page_Down",
    "pgup": "Page_Up",
    "prior": "Page_Up",
    "shift": "Shift_L",
    "super": "Super_L",
}

_BY_LOWER_NAME: dict[str, int] = {
    **{name.lower(): code for name, code in KEYSYMS.items()},
    **{alias: KEYSYMS[name] for alias, name in ALIASES.items()},
}

BUTTONS: dict[str, int] = {
    "left": 1,
    "middle": 2,
    "right": 4,
    "wheel_up": 8,
    "wheel_down": 16,
}

_BUTTON_NAMES = cv.ensure_list(cv.one_of(*BUTTONS, lower=True))


_keysym_number = cv.hex_int_range(min=0, max=_KEYSYM_MAX)


# Keep in step with keysym_for_codepoint() in vnc_client.cpp.
def _character_keysym(char: str) -> HexInt:
    code = ord(char)
    if 0x20 <= code <= 0x7E or 0xA0 <= code <= 0xFF:
        return HexInt(code)
    if code <= 0x9F:
        raise cv.Invalid(
            f"Control character U+{code:04X} is not a key. Use a key name such as Return, Tab or BackSpace"
        )
    return HexInt(_UNICODE_KEYSYM | code)


def keysym(value: object) -> HexInt:
    """An X11 keysym from a key name, a single character or a number.

    An unquoted digit 0 to 9 is read as that digit, as in tinyusb_keyboard. Names are matched
    without regard to case, but a single character is always that character, so "a" and "A" differ.
    """
    if isinstance(value, bool):
        raise cv.Invalid(
            'YAML read this key as a boolean. Put it in quotes, for example key: "y"'
        )
    if isinstance(value, int):
        if 0 <= value <= 9:
            return HexInt(ord("0") + value)
        return _keysym_number(value)
    if not isinstance(value, str):
        raise cv.Invalid(
            f"Expected a key name, a single character or a keysym number, got {value!r}"
        )
    if len(value) == 1:
        return _character_keysym(value)
    lower = value.lower()
    if lower.startswith("0x"):
        return _keysym_number(value)
    if (code := _BY_LOWER_NAME.get(lower)) is not None:
        return HexInt(code)
    raise cv.Invalid(
        f"Unknown key '{value}'. Use a single character, a keysym number such as 0xFF0D, "
        f""
        "or a key name such as Return, Escape, Tab, BackSpace, Delete, Left, Page_Up, F1 or Control_L"
    )


def button_mask(value: object) -> int:
    """A pointer button mask from a number 0 to 255, or one or more button names."""
    if isinstance(value, bool):
        raise cv.Invalid(
            f"Expected a button mask or button names ({', '.join(BUTTONS)}), got a boolean"
        )
    if isinstance(value, int):
        return cv.uint8_t(value)
    mask = 0
    for name in _BUTTON_NAMES(value):
        mask |= BUTTONS[name]
    return mask
