"""Tests for user-defined action field metadata (description / example)."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.api import validate_variable
from esphome.config_validation import Invalid
from esphome.core import CORE

CONFIG = "tests/component_tests/api/test_action_metadata.yaml"
CONFIG_SHORTHAND = "tests/component_tests/api/test_action_metadata_shorthand.yaml"


def test_metadata_is_passed_to_set_metadata(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Actions with metadata emit one set_metadata call with flash literals."""
    main_cpp = generate_main(CONFIG)

    assert main_cpp.count("set_metadata(") == 1
    assert (
        'set_metadata("Play an RTTTL melody on the buzzer", '
        '{"RTTTL melody string", nullptr}, '
        '{"two_short:d=4,o=5,b=100:16e6,16e6", nullptr})' in main_cpp
    )
    assert "USE_API_USER_DEFINED_ACTION_METADATA" in {d.name for d in CORE.defines}


def test_shorthand_variables_emit_no_metadata(
    generate_main: Callable[[str | Path], str],
) -> None:
    """The name: type shorthand emits no metadata call and no define."""
    main_cpp = generate_main(CONFIG_SHORTHAND)

    assert "set_metadata(" not in main_cpp
    assert "USE_API_USER_DEFINED_ACTION_METADATA" not in {d.name for d in CORE.defines}


def test_variable_shorthand_normalizes_to_mapping() -> None:
    """A bare type string validates to the mapping form."""
    assert validate_variable("string") == {"type": "string"}


def test_variable_mapping_requires_type() -> None:
    """The mapping form without type raises."""
    with pytest.raises(Invalid):
        validate_variable({"description": "no type given"})


def test_variable_rejects_unknown_keys() -> None:
    """Unknown keys in the mapping form raise."""
    with pytest.raises(Invalid):
        validate_variable({"type": "string", "selector": "text"})


def test_variable_rejects_invalid_type() -> None:
    """An unknown variable type raises in both forms."""
    with pytest.raises(Invalid):
        validate_variable("stringy")
    with pytest.raises(Invalid):
        validate_variable({"type": "stringy"})
