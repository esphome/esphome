"""Tests for the sensor value list filter codegen."""

from collections.abc import Callable
import re


def test_value_list_filters_share_progmem_tables(
    generate_main: Callable[[str], str],
) -> None:
    """Identical value lists share one PROGMEM table; the NaN throttle stays array free."""
    main_cpp = generate_main("tests/component_tests/sensor/test_value_list.yaml")

    tables = re.findall(
        r"static constexpr TemplatableFn<float> (\w+)\[\] PROGMEM", main_cpp
    )
    # Every table ends with an empty entry so the filters store no count.
    assert main_cpp.count("TemplatableFn<float>()}") == len(tables)
    assert len(tables) == 3  # single, shared list, lambda
    single, shared, lam = tables
    assert f"sensor::FilterOutValueFilter({single});" in main_cpp
    assert main_cpp.count(f"sensor::FilterOutValueFilter({shared});") == 2
    assert f"sensor::FilterOutValueFilter({lam});" in main_cpp
    assert f"sensor::ThrottleWithPriorityFilter(1000, {shared});" in main_cpp
    assert "sensor::ThrottleWithPriorityNanFilter(1000);" in main_cpp
    assert "return 42.0;" in main_cpp
