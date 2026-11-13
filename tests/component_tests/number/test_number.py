"""Tests for the number component codegen."""

from collections.abc import Callable
from pathlib import Path

from esphome.components import number
from esphome.core import CORE, ID
from esphome.cpp_generator import MockObj, RawExpression


def test_default_mode_is_not_emitted(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Mode auto is the C++ initializer, so only a non default mode is set."""
    main_cpp = generate_main(component_config_path("mode.yaml"))

    assert "auto_number->traits.set_mode(" not in main_cpp
    assert "explicit_auto_number->traits.set_mode(" not in main_cpp
    assert "box_number->traits.set_mode(number::NUMBER_MODE_BOX);" in main_cpp


def test_range_table_only_for_shared_ranges(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """A range used by enough numbers gets one PROGMEM table; others keep the setters."""
    main_cpp = generate_main(component_config_path("range.yaml"))

    assert (
        "static constexpr number::NumberRange number_numberrange_id[] PROGMEM = "
        "{{0.0f, 10.0f, 1.0f}};" in main_cpp
    )
    assert main_cpp.count("number::NumberRange ") == 1
    for var in ("shared_1", "shared_2", "shared_3"):
        assert f"{var}->set_range(number_numberrange_id);" in main_cpp
    assert "single->set_range(" not in main_cpp
    assert "single->traits.set_min_value(-5.0f);" in main_cpp
    assert "single->traits.set_max_value(5.0f);" in main_cpp
    assert "single->traits.set_step(0.5f);" in main_cpp


def test_range_with_lambda_bounds_uses_setters() -> None:
    """Runtime bounds (lvgl arc/bar lambdas) can't go in a PROGMEM table."""
    var = MockObj("lambda_number", "->")
    range_id = ID("lambda_range", is_declaration=True, type=number.NumberRange)
    number._add_range(var, range_id, RawExpression("get_min()"), 10, 1)

    main_cpp = CORE.cpp_main_section
    assert "lambda_number->traits.set_min_value(get_min());" in main_cpp
    assert "lambda_number->traits.set_max_value(10);" in main_cpp
    assert "lambda_number->traits.set_step(1);" in main_cpp
    assert "set_range(" not in main_cpp
