"""Tests that the api component only emits setters for non default values."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize("config_file", ["bare.yaml", "defaults.yaml"])
def test_default_values_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Port 6053, a 15 min reboot timeout, 100 ms batch delay and backlog 4 are C++ initializers.

    Both the schema defaults and the same values written explicitly take the skip path.
    """
    main_cpp = generate_main(component_config_path(config_file))

    assert "api_apiserver_id->set_port(" not in main_cpp
    assert "api_apiserver_id->set_reboot_timeout(" not in main_cpp
    assert "api_apiserver_id->set_batch_delay(" not in main_cpp
    assert "api_apiserver_id->set_listen_backlog(" not in main_cpp


def test_custom_values_are_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Non default values still reach the C++ setters."""
    main_cpp = generate_main(component_config_path("custom.yaml"))

    assert "api_apiserver_id->set_port(6054);" in main_cpp
    assert "api_apiserver_id->set_reboot_timeout(0);" in main_cpp
    assert "api_apiserver_id->set_batch_delay(0);" in main_cpp
    assert "api_apiserver_id->set_listen_backlog(2);" in main_cpp


def test_esp8266_listen_backlog_is_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The ESP8266 schema default of 1 differs from the C++ initializer, so it is set."""
    main_cpp = generate_main(component_config_path("esp8266.yaml"))

    assert "api_apiserver_id->set_listen_backlog(1);" in main_cpp
