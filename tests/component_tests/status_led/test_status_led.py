"""Tests for status_led."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_status_led_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test status_led generation."""
    main_cpp = generate_main(component_config_path("status_led_test.yaml"))
    assert (
        "alignas(status_led::StatusLED) static unsigned char status_led__status_led_statusled_id__pstorage[sizeof(status_led::StatusLED)];"
        in main_cpp
    )
    assert (
        "static status_led::StatusLED *const status_led_statusled_id = reinterpret_cast<status_led::StatusLED *>(status_led__status_led_statusled_id__pstorage);"
        in main_cpp
    )
    assert "new(status_led_statusled_id) status_led::StatusLED(" in main_cpp
