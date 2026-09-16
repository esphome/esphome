"""Tests for the number component codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_mode_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Mode auto is the C++ initializer, so only a non default mode is set."""
    main_cpp = generate_main(component_config_path("mode.yaml"))

    assert "auto_number->traits.set_mode(" not in main_cpp
    assert "explicit_auto_number->traits.set_mode(" not in main_cpp
    assert "box_number->traits.set_mode(number::NUMBER_MODE_BOX);" in main_cpp
