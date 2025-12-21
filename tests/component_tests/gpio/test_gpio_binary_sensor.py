"""Tests for the GPIO binary sensor component."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest


def test_gpio_binary_sensor_basic_setup(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    When the GPIO binary sensor is set in the yaml file, it should be registered in main
    """
    main_cpp = generate_main("tests/component_tests/gpio/test_gpio_binary_sensor.yaml")

    assert "new gpio::GPIOBinarySensor();" in main_cpp
    assert "App.register_binary_sensor" in main_cpp
    # set_use_interrupt(true) should NOT be generated (uses C++ default)
    assert "bs_gpio->set_use_interrupt(true);" not in main_cpp
    assert "bs_gpio->set_interrupt_type(gpio::INTERRUPT_ANY_EDGE);" in main_cpp


def test_gpio_binary_sensor_esp8266_gpio16_disables_interrupt(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """
    Test that ESP8266 GPIO16 automatically disables interrupt mode with a warning
    """
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_binary_sensor_esp8266.yaml"
    )

    # Check that interrupt is disabled for GPIO16
    assert "bs_gpio16->set_use_interrupt(false);" in main_cpp

    # Check that the warning was logged
    assert "GPIO16 on ESP8266 doesn't support interrupts" in caplog.text
    assert "Falling back to polling mode" in caplog.text


def test_gpio_binary_sensor_esp8266_other_pins_use_interrupt(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    Test that ESP8266 pins other than GPIO16 still use interrupt mode
    """
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_binary_sensor_esp8266.yaml"
    )

    # GPIO5 should still use interrupts (default, so no setter call)
    assert "bs_gpio5->set_use_interrupt(true);" not in main_cpp
    assert "bs_gpio5->set_interrupt_type(gpio::INTERRUPT_ANY_EDGE);" in main_cpp


def test_gpio_binary_sensor_explicit_polling_mode(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    Test that explicitly setting use_interrupt: false works
    """
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_binary_sensor_polling.yaml"
    )

    assert "bs_polling->set_use_interrupt(false);" in main_cpp
