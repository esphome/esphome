"""Tests that the api component only emits setters for non default values."""

from collections.abc import Callable
from pathlib import Path


def test_default_values_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Port 6053, a 15 min reboot timeout and 100 ms batch delay are C++ initializers."""
    main_cpp = generate_main(component_config_path("defaults.yaml"))

    assert "api_apiserver_id->set_port(" not in main_cpp
    assert "api_apiserver_id->set_reboot_timeout(" not in main_cpp
    assert "api_apiserver_id->set_batch_delay(" not in main_cpp


def test_custom_values_are_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Non default values still reach the C++ setters."""
    main_cpp = generate_main(component_config_path("custom.yaml"))

    assert "api_apiserver_id->set_port(6054);" in main_cpp
    assert "api_apiserver_id->set_reboot_timeout(0);" in main_cpp
    assert "api_apiserver_id->set_batch_delay(0);" in main_cpp
