"""Tests that light codegen skips setters for default values."""

from collections.abc import Callable
from pathlib import Path


def test_default_flash_length_and_empty_effects_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A 0 ms flash transition and an empty effect list match the C++ defaults."""
    main_cpp = generate_main(component_config_path("transitions.yaml"))

    assert "plain_light->set_flash_transition_length(" not in main_cpp
    assert "plain_light->add_effects(" not in main_cpp
    assert "bare_light->set_flash_transition_length(" not in main_cpp
    assert "bare_light->add_effects(" not in main_cpp
    assert "fancy_light->set_flash_transition_length(500);" in main_cpp
    assert "fancy_light->add_effects({" in main_cpp
