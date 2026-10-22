"""Tests for optional user-defined action variables (required/default)."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome.components.api import (
    _action_strings,
    _has_optional_args,
    validate_variable,
)
from esphome.config_validation import Invalid
from esphome.core import CORE

CONFIG = "tests/component_tests/api/test_action_optional_args.yaml"
CONFIG_ESP8266 = "tests/component_tests/api/test_action_optional_args_esp8266.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"


def test_defaults_are_emitted_in_the_string_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each argument gains a trailing default slot: "" marks optional without a default."""
    main_cpp = generate_main(CONFIG)

    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = {"
        '"play_buzzer", "song_str", "", "notes", "", "octave", "5", '
        '"bpm", "100", "loud", "true"};' in main_cpp
    )
    # An all-required action still carries the default slots (as nullptr)
    assert (
        "static constexpr const char * api_action1_strings[] PROGMEM = {"
        '"plain_action", "value", nullptr};' in main_cpp
    )
    assert "USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS" in {d.name for d in CORE.defines}


def test_esp8266_defaults_are_progmem_arrays(
    generate_main: Callable[[str | Path], str],
) -> None:
    """On ESP8266 each default is its own PROGMEM array, referenced from the table."""
    main_cpp = generate_main(CONFIG_ESP8266)

    def array_for(text: str) -> str:
        match = re.search(
            rf'static constexpr char (progmem_str\w*)\[\] PROGMEM = "{text}";', main_cpp
        )
        assert match, text
        return match.group(1)

    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = {"
        f"{array_for('play_buzzer')}, {array_for('song_str')}, {array_for('')}, "
        f"{array_for('notes')}, {array_for('')}, {array_for('octave')}, {array_for('5')}, "
        f"{array_for('bpm')}, {array_for('100')}, {array_for('loud')}, "
        f"{array_for('true')}}};" in main_cpp
    )


def test_required_variables_emit_no_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(CONFIG_SHORTHAND)

    assert "USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS" not in {
        d.name for d in CORE.defines
    }


def test_action_strings_orders_default_after_metadata() -> None:
    conf = {
        "action": "a",
        "description": "d",
        "variables": {
            "b": {"type": "int", "example": "1", "default": 5},
            "c": {"type": "bool", "default": False},
        },
    }
    assert _action_strings(conf, has_metadata=True, has_optional=True) == [
        "a",
        "d",
        "b",
        None,
        "1",
        "5",
        "c",
        None,
        None,
        "false",
    ]


@pytest.mark.parametrize(
    ("var", "expected"),
    [
        ({"type": "int", "required": False}, ""),
        ({"type": "string", "default": ""}, ""),
        ({"type": "float", "default": 5}, "5.0"),
    ],
)
def test_default_slot_marks_optional_and_keeps_the_type(
    var: dict, expected: str
) -> None:
    actions = [{"action": "a", "variables": {"b": validate_variable(var)}}]
    assert _has_optional_args(actions)
    assert _action_strings(actions[0], has_metadata=False, has_optional=True) == [
        "a",
        "b",
        expected,
    ]


@pytest.mark.parametrize(
    ("var", "expected"),
    [
        ({"type": "int", "default": "5"}, 5),
        ({"type": "int", "default": -(2**31)}, -(2**31)),
        ({"type": "int", "default": 2**31 - 1}, 2**31 - 1),
        ({"type": "bool", "default": "true"}, True),
        ({"type": "float", "default": "1.5"}, 1.5),
        ({"type": "float", "default": 3.4028235e38}, 3.4028235e38),
        ({"type": "string", "default": "0123"}, "0123"),
    ],
)
def test_default_is_validated_against_type(var: dict, expected: object) -> None:
    assert validate_variable(var)["default"] == expected


@pytest.mark.parametrize(
    "var",
    [
        {"type": "int[]", "required": False},
        {"type": "string[]", "default": "x"},
        {"type": "int", "default": 5, "required": True},
        {"type": "int", "default": "not_a_number"},
        # ExecuteServiceArgument.int_ is a signed 32-bit value
        {"type": "int", "default": 2**31},
        {"type": "int", "default": -(2**31) - 1},
        # A boolean is an int in Python but "true" is not a number on the wire
        {"type": "int", "default": True},
        {"type": "float", "default": False},
        # ExecuteServiceArgument.float_ is a 32-bit float
        {"type": "float", "default": 1e40},
        {"type": "float", "default": "nan"},
        {"type": "float", "default": "inf"},
        # A string default is what the user typed, and a C string in flash
        {"type": "string", "default": 123},
        {"type": "string", "default": "a\0b"},
        {"type": "string", "default": "\0a"},
        {"type": "float", "default": -1e40},
    ],
)
def test_variable_rejects_invalid_optionality(var: dict) -> None:
    with pytest.raises(Invalid):
        validate_variable(var)
