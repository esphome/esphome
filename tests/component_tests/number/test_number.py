"""Tests for the number component codegen."""

from collections.abc import Callable
from pathlib import Path


def test_default_mode_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Mode auto is the C++ initializer, so only a non default mode is set."""
    main_cpp = generate_main(component_config_path("mode.yaml"))

    assert "auto_number->traits.set_mode(" not in main_cpp
    assert "explicit_auto_number->traits.set_mode(" not in main_cpp
    assert "box_number->traits.set_mode(number::NUMBER_MODE_BOX);" in main_cpp


def test_value_range_runs_a_generated_function(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """on_value_range builds a callback component with the number and its bounds function."""
    main_cpp = generate_main(component_config_path("value_range.yaml"))

    assert (
        "new(level_range) FireCallback<esphome__automation_id__fn, number::ValueRangeBase>("
        "level, esphome__level_range__fn);" in main_cpp
    )
    assert "return ValueRange{10.0f, 90.0f};" in main_cpp
    assert "Automation<float>" not in main_cpp
