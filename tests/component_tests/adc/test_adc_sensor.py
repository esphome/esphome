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


def test_zephyr_adc_channel_node_name_is_hex(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Zephyr looks the channel node up by its unit address read as hex, so channel
    22 must be `channel@16`; `channel@22` is address 0x22 and the lookup misses."""
    from esphome.components.zephyr import zephyr_data  # noqa: PLC0415
    from esphome.components.zephyr.const import KEY_OVERLAY  # noqa: PLC0415

    generate_main("tests/component_tests/adc/test_adc_zephyr_ra4m1.yaml")
    overlay = "".join(zephyr_data()[KEY_OVERLAY].values())

    assert "channel@9 {" in overlay  # A0, AN009
    assert "channel@16 {" in overlay  # A5, AN022
    assert "reg = <22>;" in overlay
    assert "channel@22 {" not in overlay
