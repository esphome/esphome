"""Tests for the template switch codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_assumed_state_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """assumed_state false is the C++ initializer, so only true is set."""
    main_cpp = generate_main(component_config_path("switch_assumed_state.yaml"))

    assert "plain_switch->set_assumed_state(" not in main_cpp
    assert "assumed_switch->set_assumed_state(true);" in main_cpp
