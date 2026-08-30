"""Tests for user-defined action names: PROGMEM storage and the length limit."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import ACTION_NAME_MAX_LENGTH, validate_action_name
from esphome.config_validation import Invalid
from esphome.cpp_generator import safe_exp
from esphome.helpers import fnv1_hash

CONFIG = "tests/component_tests/api/test_homeassistant_action.yaml"


def test_action_name_is_progmem_and_key_is_hashed_at_codegen(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(CONFIG)

    assert (
        'static constexpr char api_action0_name[] PROGMEM = "zero_copy_args";'
        in main_cpp
    )
    assert f"(api_action0_name, {safe_exp(fnv1_hash('zero_copy_args'))}, " in main_cpp


@pytest.mark.parametrize(
    ("length", "valid"),
    [(ACTION_NAME_MAX_LENGTH, True), (ACTION_NAME_MAX_LENGTH + 1, False)],
)
def test_action_name_length_limit(length: int, valid: bool) -> None:
    name = "a" * length
    if valid:
        assert validate_action_name(name) == name
    else:
        with pytest.raises(Invalid):
            validate_action_name(name)
