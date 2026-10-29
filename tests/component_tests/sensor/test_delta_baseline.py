"""Tests for the sensor delta filter baseline."""

from collections.abc import Callable


def test_delta_baseline_number(generate_main: Callable[[str], str]) -> None:
    """A plain number baseline, including 0, becomes a lambda returning it."""
    main_cpp = generate_main("tests/component_tests/sensor/test_delta_baseline.yaml")

    assert "set_baseline([](float x) -> float {\n    return 20.0f;\n});" in main_cpp
    assert "set_baseline([](float x) -> float {\n    return 0.0f;\n});" in main_cpp
    assert "return x * 2.0f;" in main_cpp
