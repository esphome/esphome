"""Tests for the vnc keysym and pointer button validators."""

import pytest

from esphome.components.vnc.keysyms import (
    ALIASES,
    BUTTONS,
    KEYSYMS,
    button_mask,
    keysym,
)
import esphome.config_validation as cv
from esphome.core import HexInt


@pytest.mark.parametrize(
    ("name", "code"),
    [
        ("BackSpace", 0xFF08),
        ("Tab", 0xFF09),
        ("Return", 0xFF0D),
        ("Pause", 0xFF13),
        ("Scroll_Lock", 0xFF14),
        ("Escape", 0xFF1B),
        ("Home", 0xFF50),
        ("Left", 0xFF51),
        ("Up", 0xFF52),
        ("Right", 0xFF53),
        ("Down", 0xFF54),
        ("Page_Up", 0xFF55),
        ("Page_Down", 0xFF56),
        ("End", 0xFF57),
        ("Print", 0xFF61),
        ("Insert", 0xFF63),
        ("Menu", 0xFF67),
        ("Num_Lock", 0xFF7F),
        ("KP_Enter", 0xFF8D),
        ("F1", 0xFFBE),
        ("F2", 0xFFBF),
        ("F12", 0xFFC9),
        ("Shift_L", 0xFFE1),
        ("Shift_R", 0xFFE2),
        ("Control_L", 0xFFE3),
        ("Control_R", 0xFFE4),
        ("Caps_Lock", 0xFFE5),
        ("Meta_L", 0xFFE7),
        ("Meta_R", 0xFFE8),
        ("Alt_L", 0xFFE9),
        ("Alt_R", 0xFFEA),
        ("Super_L", 0xFFEB),
        ("Super_R", 0xFFEC),
        ("Delete", 0xFFFF),
        ("space", 0x20),
        ("ISO_Left_Tab", 0xFE20),
    ],
)
def test_keysym_names(name: str, code: int) -> None:
    """Every documented name maps to its X11 keysym, in any case."""
    assert KEYSYMS[name] == code
    assert keysym(name) == code
    assert keysym(name.lower()) == code
    assert keysym(name.upper()) == code


def test_function_keys_are_contiguous() -> None:
    """F1 to F12 run without gaps."""
    assert [KEYSYMS[f"F{n}"] for n in range(1, 13)] == list(range(0xFFBE, 0xFFCA))


@pytest.mark.parametrize(
    ("alias", "code"),
    [
        ("enter", 0xFF0D),
        ("Enter", 0xFF0D),
        ("esc", 0xFF1B),
        ("ESC", 0xFF1B),
        ("del", 0xFFFF),
        ("ins", 0xFF63),
        ("pgup", 0xFF55),
        ("PgDn", 0xFF56),
        ("ctrl", 0xFFE3),
        ("shift", 0xFFE1),
        ("alt", 0xFFE9),
        ("super", 0xFFEB),
        ("backspace", 0xFF08),
        ("tab", 0xFF09),
        ("return", 0xFF0D),
        ("RETURN", 0xFF0D),
        ("page_up", 0xFF55),
        ("Prior", 0xFF55),
        ("Next", 0xFF56),
    ],
)
def test_keysym_aliases(alias: str, code: int) -> None:
    """Short aliases and lowercase names resolve to the X11 keysym."""
    assert keysym(alias) == code


def test_lowercase_names_are_unique() -> None:
    """No two names or aliases collide once lowercased."""
    names = [name.lower() for name in KEYSYMS] + list(ALIASES)
    assert len(names) == len(set(names))
    assert all(alias == alias.lower() for alias in ALIASES)
    assert all(name in KEYSYMS for name in ALIASES.values())


@pytest.mark.parametrize(("digit", "code"), [(n, 0x30 + n) for n in range(10)])
def test_keysym_unquoted_digit(digit: int, code: int) -> None:
    """An unquoted digit 0 to 9 is that digit, not a raw keysym."""
    assert keysym(digit) == code


@pytest.mark.parametrize("code", [10, 0x41, 0xFF0D, 0x01000041, 0x1FFFFFFF])
def test_keysym_int_passes_through(code: int) -> None:
    """Other integers in range are raw keysyms."""
    assert keysym(code) == code


@pytest.mark.parametrize("code", [-1, 0x20000000])
def test_keysym_int_out_of_range(code: int) -> None:
    """Integers outside the keysym range are rejected."""
    with pytest.raises(cv.Invalid, match="value must be at"):
        keysym(code)


@pytest.mark.parametrize("value", [True, False])
def test_keysym_bool_rejected(value: bool) -> None:
    """YAML booleans such as y and n ask the user to quote the key."""
    with pytest.raises(cv.Invalid, match="quotes"):
        keysym(value)


@pytest.mark.parametrize(
    ("char", "code"),
    [
        (" ", 0x20),
        ("a", 0x61),
        ("A", 0x41),
        ("0", 0x30),
        ("9", 0x39),
        ("~", 0x7E),
        (" ", 0xA0),
        ("é", 0xE9),
        ("ÿ", 0xFF),
        ("€", 0x010020AC),
        ("Ā", 0x01000100),
        ("😀", 0x0101F600),
    ],
)
def test_keysym_single_character(char: str, code: int) -> None:
    """Latin-1 characters are their code point, others use the Unicode keysym range."""
    assert keysym(char) == code


def test_keysym_single_character_is_case_sensitive() -> None:
    """A single letter is never folded by the case-insensitive name lookup."""
    assert keysym("a") != keysym("A")


@pytest.mark.parametrize("char", ["\x00", "\n", "\t", "\x1f", "\x7f", "\x80", "\x9f"])
def test_keysym_control_character_rejected(char: str) -> None:
    """Control characters are not keys."""
    with pytest.raises(cv.Invalid, match="Control character"):
        keysym(char)


@pytest.mark.parametrize(
    ("value", "code"),
    [
        ("0xFF0D", 0xFF0D),
        ("0xff0d", 0xFF0D),
        ("0XFF0D", 0xFF0D),
        ("0x0", 0),
        ("0x1FFFFFFF", 0x1FFFFFFF),
    ],
)
def test_keysym_hex_string(value: str, code: int) -> None:
    """Hexadecimal strings are raw keysyms."""
    assert keysym(value) == code


def test_keysym_hex_string_out_of_range() -> None:
    """A hexadecimal keysym above the range is rejected."""
    with pytest.raises(cv.Invalid, match="value must be at"):
        keysym("0x20000000")


@pytest.mark.parametrize("value", ["0x", "0xZZ", "0x12G"])
def test_keysym_hex_string_invalid(value: str) -> None:
    """A malformed hexadecimal string is rejected."""
    with pytest.raises(cv.Invalid, match="cannot parse"):
        keysym(value)


@pytest.mark.parametrize("value", ["", "Foo", "Return2", "65", "F13", "left_shift"])
def test_keysym_unknown_name(value: str) -> None:
    """Unknown names list a few valid ones."""
    with pytest.raises(cv.Invalid, match="Unknown key") as exc_info:
        keysym(value)
    assert "Return" in str(exc_info.value)


@pytest.mark.parametrize("value", [None, 1.5, ["a"], {"key": "a"}])
def test_keysym_wrong_type(value: object) -> None:
    """Values that are not strings or integers are rejected."""
    with pytest.raises(cv.Invalid, match="Expected a key name"):
        keysym(value)


def test_keysym_returns_hex_int() -> None:
    """Keysyms render as hexadecimal in the generated code."""
    assert isinstance(keysym("Return"), HexInt)
    assert isinstance(keysym("a"), HexInt)
    assert isinstance(keysym(5), HexInt)
    assert isinstance(keysym(0xFF0D), HexInt)
    assert isinstance(keysym("0xFF0D"), HexInt)


def test_buttons_table() -> None:
    """Button names map to the RFB pointer mask bits."""
    assert BUTTONS == {
        "left": 1,
        "middle": 2,
        "right": 4,
        "wheel_up": 8,
        "wheel_down": 16,
    }


@pytest.mark.parametrize(
    ("value", "mask"),
    [
        (0, 0),
        (1, 1),
        (255, 255),
        ("left", 1),
        ("LEFT", 1),
        ("middle", 2),
        ("right", 4),
        ("wheel_up", 8),
        ("wheel_down", 16),
        (["left"], 1),
        (["left", "right"], 5),
        (["left", "middle", "right", "wheel_up", "wheel_down"], 31),
        (["left", "left"], 1),
        ([], 0),
        (None, 0),
    ],
)
def test_button_mask(value: object, mask: int) -> None:
    """Masks are numbers or the OR of the named buttons."""
    assert button_mask(value) == mask


@pytest.mark.parametrize("value", [-1, 256])
def test_button_mask_out_of_range(value: int) -> None:
    """Numeric masks must fit in one byte."""
    with pytest.raises(cv.Invalid, match="value must be at"):
        button_mask(value)


@pytest.mark.parametrize("value", ["back", ["left", "back"], [1]])
def test_button_mask_unknown_name(value: object) -> None:
    """Unknown button names are rejected."""
    with pytest.raises(cv.Invalid, match="Unknown value"):
        button_mask(value)


@pytest.mark.parametrize("value", [True, False])
def test_button_mask_bool_rejected(value: bool) -> None:
    """Booleans are not silently read as button 1 or no buttons."""
    with pytest.raises(cv.Invalid, match="boolean"):
        button_mask(value)
