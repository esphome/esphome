"""Tests for optional user-defined action variables (required/default)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import (
    _action_strings,
    _has_optional_args,
    _validate_optional_arg_positions,
    validate_variable,
)
from esphome.config_validation import Invalid
from esphome.core import CORE

CONFIG = "tests/component_tests/api/test_action_optional_args.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"


def test_defaults_are_emitted_in_the_string_table(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Each argument gains a trailing default slot; optional flags travel as one mask."""
    main_cpp = generate_main(CONFIG)

    assert 'static constexpr char api_action_str4[] PROGMEM = "5";' in main_cpp
    assert 'static constexpr char api_action_str6[] PROGMEM = "100";' in main_cpp
    assert 'static constexpr char api_action_str8[] PROGMEM = "true";' in main_cpp
    assert (
        "static constexpr const char * api_action0_strings[] PROGMEM = {"
        "api_action_str0, api_action_str1, nullptr, api_action_str2, nullptr, "
        "api_action_str3, api_action_str4, api_action_str5, api_action_str6, "
        "api_action_str7, api_action_str8};" in main_cpp
    )
    # An all-required action still carries the default slots (as nullptr)
    assert (
        "static constexpr const char * api_action1_strings[] PROGMEM = {"
        "api_action_str9, api_action_str10, nullptr};" in main_cpp
    )
    # All five variables of play_buzzer are optional; plain_action emits no mask
    assert main_cpp.count("set_optional_args_mask(") == 1
    assert "set_optional_args_mask(31)" in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS" in {d.name for d in CORE.defines}


def test_required_variables_emit_no_mask_or_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG_SHORTHAND)

    assert "set_optional_args_mask(" not in main_cpp
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


def test_required_false_is_optional_without_default() -> None:
    actions = [{"action": "a", "variables": {"b": {"type": "int", "required": False}}}]
    assert _has_optional_args(actions)
    assert _action_strings(actions[0], has_metadata=False, has_optional=True) == [
        "a",
        "b",
        None,
    ]


def test_empty_string_default_is_unset() -> None:
    conf = {"action": "a", "variables": {"b": {"type": "string", "default": ""}}}
    assert _action_strings(conf, has_metadata=False, has_optional=True) == [
        "a",
        "b",
        None,
    ]


def test_default_is_validated_against_type() -> None:
    assert validate_variable({"type": "int", "default": "5"})["default"] == 5
    assert validate_variable({"type": "bool", "default": "true"})["default"] is True
    with pytest.raises(Invalid):
        validate_variable({"type": "int", "default": "not_a_number"})


def test_float_default_keeps_decimal_point() -> None:
    var = validate_variable({"type": "float", "default": 5})
    conf = {"action": "a", "variables": {"b": var}}
    assert _action_strings(conf, has_metadata=False, has_optional=True) == [
        "a",
        "b",
        "5.0",
    ]


def test_array_variables_cannot_be_optional() -> None:
    with pytest.raises(Invalid):
        validate_variable({"type": "int[]", "required": False})
    with pytest.raises(Invalid):
        validate_variable({"type": "string[]", "default": "x"})


def test_default_with_required_true_raises() -> None:
    with pytest.raises(Invalid):
        validate_variable({"type": "int", "default": 5, "required": True})


def test_optional_variable_past_mask_width_raises() -> None:
    variables = {f"v{i}": {"type": "int"} for i in range(32)}
    variables["late"] = {"type": "int", "default": 1}
    config = {"actions": [{"action": "a", "variables": variables}]}
    with pytest.raises(Invalid, match="first 32 variables"):
        _validate_optional_arg_positions(config)
