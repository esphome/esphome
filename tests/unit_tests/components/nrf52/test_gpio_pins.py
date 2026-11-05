"""GPIO pin numbers allowed for each nRF SoC variant."""

import pytest

from esphome.components.nrf52.const import (
    VARIANT_NRF54L15,
    VARIANT_NRF54LM20,
    VARIANT_NRF51822,
    VARIANT_NRF52840,
)
from esphome.components.nrf52.framework import set_nrf52_variant
from esphome.components.nrf52.gpio import validate_gpio_pin
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("variant", "pin", "number"),
    [
        (VARIANT_NRF51822, "P0.31", 31),
        (VARIANT_NRF52840, "P0.31", 31),
        (VARIANT_NRF52840, "P1.15", 47),
        (VARIANT_NRF54L15, "P0.06", 6),
        (VARIANT_NRF54L15, "P1.16", 48),
        (VARIANT_NRF54L15, "P2.10", 74),
        (VARIANT_NRF54LM20, "P0.09", 9),
        (VARIANT_NRF54LM20, "P1.22", 54),
        (VARIANT_NRF54LM20, "P1.31", 63),
        (VARIANT_NRF54LM20, "P2.10", 74),
        (VARIANT_NRF54LM20, "P3.12", 108),
    ],
)
def test_valid_pins(variant: str, pin: str, number: int) -> None:
    set_nrf52_variant(variant)
    assert validate_gpio_pin(pin) == number


@pytest.mark.parametrize(
    ("variant", "pin"),
    [
        (VARIANT_NRF51822, "P1.00"),
        (VARIANT_NRF52840, "P1.16"),
        (VARIANT_NRF52840, "P2.00"),
        (VARIANT_NRF54L15, "P0.07"),
        (VARIANT_NRF54L15, "P1.17"),
        (VARIANT_NRF54L15, "P2.11"),
        (VARIANT_NRF54L15, "P3.00"),
        (VARIANT_NRF54LM20, "P0.10"),
        (VARIANT_NRF54LM20, "P2.11"),
        (VARIANT_NRF54LM20, "P3.13"),
        (VARIANT_NRF54LM20, "P4.00"),
    ],
)
def test_invalid_pins(variant: str, pin: str) -> None:
    set_nrf52_variant(variant)
    with pytest.raises(cv.Invalid, match=f"{variant}: Invalid pin number"):
        validate_gpio_pin(pin)
