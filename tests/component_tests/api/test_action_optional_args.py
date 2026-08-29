"""Tests for optional user-defined action variables (required/default)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import validate_variable
from esphome.config_validation import Invalid
from esphome.core import CORE

CONFIG = "tests/component_tests/api/test_action_optional_args.yaml"


def test_optional_args_are_passed_to_set_optional_args(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Optional variables emit one set_optional_args call with mask and defaults."""
    main_cpp = generate_main(CONFIG)

    # song_str is bit 0, notes is bit 1; args with defaults are not in the mask
    assert main_cpp.count("set_optional_args(") == 1
    assert 'set_optional_args(3, {nullptr, nullptr, "5", "100", "true"})' in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS" in {d.name for d in CORE.defines}


def test_required_variables_emit_no_optional_call(
    generate_main: Callable[[str | Path], str],
) -> None:
    """All-required actions emit no optional-args call and no define."""
    main_cpp = generate_main(
        "tests/component_tests/api/test_action_metadata_shorthand.yaml"
    )

    assert "set_optional_args(" not in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_OPTIONAL_ARGS" not in {
        d.name for d in CORE.defines
    }


def test_default_is_validated_against_type() -> None:
    """A default must validate as the declared type."""
    assert validate_variable({"type": "int", "default": "5"})["default"] == 5
    assert validate_variable({"type": "bool", "default": "true"})["default"] is True
    with pytest.raises(Invalid):
        validate_variable({"type": "int", "default": "not_a_number"})


def test_array_variables_cannot_be_optional() -> None:
    """Array types reject required/default."""
    with pytest.raises(Invalid):
        validate_variable({"type": "int[]", "required": False})
    with pytest.raises(Invalid):
        validate_variable({"type": "string[]", "default": "x"})


def test_default_with_required_true_raises() -> None:
    """A default combined with required: true raises."""
    with pytest.raises(Invalid):
        validate_variable({"type": "int", "default": 5, "required": True})
