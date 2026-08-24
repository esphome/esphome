"""Tests for the GPIO binary sensor component."""

from __future__ import annotations

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome.core import CORE

INTERRUPT_DEFINE = "USE_GPIO_BINARY_SENSOR_INTERRUPT"


def test_gpio_binary_sensor_basic_setup(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    When the GPIO binary sensor is set in the yaml file, it should be registered in main
    """
    main_cpp = generate_main("tests/component_tests/gpio/test_gpio_binary_sensor.yaml")

    assert "static gpio::GPIOBinarySensor *const" in main_cpp
    assert ") gpio::GPIOBinarySensor();" in main_cpp
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


def test_gpio_binary_sensor_interrupt_emits_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    An interrupt-mode sensor must emit the define that compiles the ISR code,
    since the platform ISR pin implementation is only built when needed
    """
    generate_main("tests/component_tests/gpio/test_gpio_binary_sensor.yaml")

    assert INTERRUPT_DEFINE in {d.name for d in CORE.defines}


def test_gpio_binary_sensor_polling_omits_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    A polling-only config must not emit the interrupt define, so the ISR code
    (and its reference to ISRInternalGPIOPin) is compiled out
    """
    generate_main("tests/component_tests/gpio/test_gpio_binary_sensor_polling.yaml")

    assert INTERRUPT_DEFINE not in {d.name for d in CORE.defines}


def test_gpio_binary_sensor_mixed_modes_emit_define(
    generate_main: Callable[[str | Path], str],
) -> None:
    """
    With one interrupt and one polling sensor, the define is emitted and the
    polling instance still opts out via its setter
    """
    main_cpp = generate_main(
        "tests/component_tests/gpio/test_gpio_binary_sensor_mixed.yaml"
    )

    assert INTERRUPT_DEFINE in {d.name for d in CORE.defines}
    assert "bs_polling->set_use_interrupt(false);" in main_cpp
    assert "bs_interrupt->set_use_interrupt" not in main_cpp


def test_gpio_binary_sensor_expander_pin_omits_define(
    generate_main: Callable[[str | Path], str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """
    An expander pin can't use interrupts: final validation falls back to
    polling and the interrupt define must not be emitted. This is the config
    that fails to link if the ISR code is compiled without an internal pin
    """
    with caplog.at_level(logging.INFO):
        main_cpp = generate_main(
            "tests/component_tests/gpio/test_gpio_binary_sensor_expander.yaml"
        )

    assert "bs_expander->set_use_interrupt(false);" in main_cpp
    assert INTERRUPT_DEFINE not in {d.name for d in CORE.defines}
    assert "falling back to polling mode" in caplog.text
