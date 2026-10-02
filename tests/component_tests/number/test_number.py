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


def test_range_table_shared_per_distinct_values(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Numbers with the same range share one PROGMEM table."""
    main_cpp = generate_main(component_config_path("range.yaml"))

    shared = (
        "number::NumberRange number_numberrange_id[] PROGMEM = {{0.0f, 10.0f, 1.0f}};"
    )
    other = (
        "number::NumberRange number_numberrange_id_3[] PROGMEM = {{-5.0f, 5.0f, 0.5f}};"
    )
    assert shared in main_cpp
    assert other in main_cpp
    assert main_cpp.count("number::NumberRange ") == 2
    assert "first_number->set_range(number_numberrange_id);" in main_cpp
    assert "same_range_number->set_range(number_numberrange_id);" in main_cpp
    assert "other_range_number->set_range(number_numberrange_id_3);" in main_cpp
    assert "traits.set_min_value(" not in main_cpp
