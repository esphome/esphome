"""Tests for the template switch codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_flags_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Only true optimistic and assumed_state are set; false is the C++ initializer."""
    main_cpp = generate_main(component_config_path("switch_defaults.yaml"))

    assert "plain_switch->set_optimistic(" not in main_cpp
    assert "plain_switch->set_assumed_state(" not in main_cpp
    assert "enabled_switch->set_optimistic(true);" in main_cpp
    assert "enabled_switch->set_assumed_state(true);" in main_cpp
