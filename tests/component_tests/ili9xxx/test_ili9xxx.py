"""Tests for the ili9xxx component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path


def test_ili9xxx_placement_new_uses_model_subclass(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Regression test for ili9xxx picking the right constructor under placement new.

    ili9xxx declares the ID as the base ``ILI9XXXDisplay`` but constructs a
    model-specific subclass (e.g. ``ILI9XXXST7789V``) via ``MODELS[...].new()``.
    Pvariable must emit placement new for the subclass — otherwise the base
    default constructor runs and the panel is left with a null init sequence
    and 0x0 dimensions, producing a silent blank screen.
    """
    main_cpp = generate_main(component_config_path("ili9xxx_test.yaml"))

    # Storage is sized for the subclass so the full object fits.
    assert "sizeof(ili9xxx::ILI9XXXST7789V)" in main_cpp
    assert "alignas(ili9xxx::ILI9XXXST7789V)" in main_cpp
    # Pointer is declared as the base type for polymorphism.
    assert "static ili9xxx::ILI9XXXDisplay *const tft_display" in main_cpp
    # Placement new runs the subclass constructor — this is the actual regression fix.
    assert "new(tft_display) ili9xxx::ILI9XXXST7789V()" in main_cpp
    # Base-class default constructor must NOT be used.
    assert "new(tft_display) ili9xxx::ILI9XXXDisplay()" not in main_cpp
