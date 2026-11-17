"""Tests for the version text sensor codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_hide_flags_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Both flags are false in C++, so only true reaches the setters."""
    main_cpp = generate_main(component_config_path("version.yaml"))

    assert "default_version->set_hide_hash(" not in main_cpp
    assert "default_version->set_hide_timestamp(" not in main_cpp
    assert "hidden_version->set_hide_hash(true);" in main_cpp
    assert "hidden_version->set_hide_timestamp(true);" in main_cpp
