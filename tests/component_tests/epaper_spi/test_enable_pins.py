"""Tests for applying the default enable pins of epaper_spi models."""

from collections.abc import Callable
from pathlib import Path
import re


def _enable_pin_numbers(main_cpp: str) -> list[int]:
    """Get the GPIO numbers passed to set_enable_pins() in the generated code."""
    match = re.search(r"epaper_display->set_enable_pins\(\{(.*)\}\);", main_cpp)
    if match is None:
        return []
    numbers = []
    for pin_id in match.group(1).split(", "):
        pin = re.search(rf"{pin_id}->set_pin\(::GPIO_NUM_(\d+)\);", main_cpp)
        assert pin is not None
        numbers.append(int(pin.group(1)))
    return numbers


def test_default_enable_pin(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The model's enable pin is applied when it is not used elsewhere."""
    main_cpp = generate_main(component_config_path("enable_pin_default.yaml"))
    assert _enable_pin_numbers(main_cpp) == [47]


def test_default_enable_pin_used_elsewhere(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A default enable pin used by a switch is left out instead of failing validation."""
    main_cpp = generate_main(component_config_path("enable_pin_used_elsewhere.yaml"))
    assert _enable_pin_numbers(main_cpp) == []
    assert "panel_power->set_pin(" in main_cpp


def test_user_enable_pins(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Enable pins given in the config are applied as given."""
    main_cpp = generate_main(component_config_path("enable_pin_test.yaml"))
    assert _enable_pin_numbers(main_cpp) == [25, 26]
