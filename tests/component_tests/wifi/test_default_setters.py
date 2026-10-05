"""Tests that wifi codegen skips setters for default values."""

from collections.abc import Callable
from pathlib import Path

import pytest


@pytest.mark.parametrize("config_file", ["bare.yaml", "defaults.yaml"])
def test_default_values_are_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """Priority 0, 90 s AP timeout, 15 min reboot, power save none, WPA2 are C++ defaults.

    Both the schema defaults and the same values written explicitly take the skip path.
    """
    main_cpp = generate_main(component_config_path(config_file))

    assert "set_priority(" not in main_cpp
    assert "set_ap_timeout(" not in main_cpp
    assert "set_reboot_timeout(" not in main_cpp
    assert "set_power_save_mode(" not in main_cpp
    assert "set_min_auth_mode(" not in main_cpp


def test_custom_values_are_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Non default values still reach the C++ setters."""
    main_cpp = generate_main(component_config_path("custom.yaml"))

    assert "set_priority(5);" in main_cpp
    assert "set_ap_timeout(120000);" in main_cpp
    assert "set_reboot_timeout(0);" in main_cpp
    assert "set_power_save_mode(wifi::WIFI_POWER_SAVE_LIGHT);" in main_cpp
    assert "set_min_auth_mode(wifi::WIFI_MIN_AUTH_MODE_WPA);" in main_cpp
