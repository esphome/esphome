"""Tests for mpip_spi configuration validation."""

from collections.abc import Callable, Generator

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_ESP32, KEY_VARIANT, VARIANTS
from esphome.components.esp32.gpio import validate_gpio_pin
from esphome.const import CONF_INPUT, CONF_OUTPUT
from esphome.core import CORE
from esphome.pins import gpio_pin_schema


@pytest.fixture
def choose_variant_with_pins() -> Generator[Callable[[list], None]]:
    """
    Set the ESP32 variant for the given model based on pins. For ESP32 only since the other platforms
    do not have variants.
    """

    def chooser(pins: list) -> None:
        for v in VARIANTS:
            try:
                CORE.data[KEY_ESP32][KEY_VARIANT] = v
                for pin in pins:
                    if pin is not None:
                        pin = gpio_pin_schema(
                            {
                                CONF_INPUT: True,
                                CONF_OUTPUT: True,
                            },
                            internal=True,
                        )(pin)
                        validate_gpio_pin(pin)
                return
            except cv.Invalid:
                continue
        raise cv.Invalid(
            f"No compatible variant found for pins: {', '.join(map(str, pins))}"
        )

    yield chooser
