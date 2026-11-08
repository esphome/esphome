"""Tests for applying the default enable pins of mipi_rgb models."""

from collections.abc import Callable
from pathlib import Path
import re


def _enable_pin_numbers(main_cpp: str) -> list[int]:
    """Get the expander pin numbers passed to set_enable_pins() in the generated code."""
    match = re.search(r"default_pins->set_enable_pins\(\{(.*)\}\);", main_cpp)
    assert match is not None
    numbers = []
    for pin_id in match.group(1).split(", "):
        pin = re.search(rf"{pin_id}->set_pin\((\d+)\);", main_cpp)
        assert pin is not None
        numbers.append(int(pin.group(1)))
    return numbers


def test_default_enable_pins(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """All of the model's enable pins are applied when none are used elsewhere."""
    main_cpp = generate_main(component_fixture_path("enable_pins_default.yaml"))
    assert _enable_pin_numbers(main_cpp) == [2, 6]


def test_default_enable_pin_used_elsewhere(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """A default enable pin used by a switch is left out instead of failing validation."""
    main_cpp = generate_main(component_fixture_path("enable_pins.yaml"))
    assert _enable_pin_numbers(main_cpp) == [6]
    assert "backlight->set_pin(" in main_cpp
