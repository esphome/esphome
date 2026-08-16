"""Tests for st77922_touch code generation."""

from collections.abc import Callable
from pathlib import Path


def test_freenove_touchscreen_generation(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Generate the Freenove display and touchscreen configuration."""
    main_cpp = generate_main(component_fixture_path("fnk0104n.yaml"))

    assert "st77922_touch::ST77922Touchscreen" in main_cpp
    assert "set_i2c_address(0x55)" in main_cpp
    assert "set_interrupt_pin" in main_cpp
    assert "set_reset_pin" in main_cpp
