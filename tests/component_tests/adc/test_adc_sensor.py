"""Tests for the ADC sensor component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest


def test_adc_temperature_pin_is_deprecated(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """`pin: TEMPERATURE` still works, but warns and points at internal_temperature."""
    main_cpp = generate_main("tests/component_tests/adc/test_adc_sensor.yaml")

    assert "adc_temperature->set_is_temperature();" in main_cpp
    assert "`pin: TEMPERATURE` is deprecated" in caplog.text
    assert "internal_temperature" in caplog.text
    assert "2027.2.0" in caplog.text


def test_adc_regular_pin_is_not_deprecated(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A normal ADC pin does not emit the temperature deprecation warning."""
    main_cpp = generate_main("tests/component_tests/adc/test_adc_sensor.yaml")

    assert "adc_voltage->set_is_temperature();" not in main_cpp
    assert caplog.text.count("`pin: TEMPERATURE` is deprecated") == 1
