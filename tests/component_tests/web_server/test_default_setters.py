"""Tests that web_server only emits setters for non default values."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize("config_file", ["bare.yaml", "defaults.yaml"])
def test_default_values_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Port 80, log on and include_internal off already live in the C++ initializers.

    Both the schema defaults and the same values written explicitly take the skip path.
    """
    main_cpp = generate_main(component_config_path(config_file))

    assert "set_port(" not in main_cpp
    assert "set_expose_log(" not in main_cpp
    assert "set_include_internal(" not in main_cpp


def test_custom_values_are_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Non default values still reach the C++ setters."""
    main_cpp = generate_main(component_config_path("custom.yaml"))

    assert "set_port(8080);" in main_cpp
    assert "set_expose_log(false);" in main_cpp
    assert "set_include_internal(true);" in main_cpp
