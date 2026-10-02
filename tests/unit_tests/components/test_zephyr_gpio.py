"""Unit tests for esphome.components.zephyr.gpio's pin-notation validators."""

from __future__ import annotations

import pytest

from esphome.components.zephyr.const import KEY_ZEPHYR
from esphome.components.zephyr.gpio import _validate_gpio_pin, pin_summary
from esphome.components.zephyr.variants import VARIANTS
import esphome.config_validation as cv
from esphome.core import CORE


def _set_zephyr_variant(variant: str | None) -> None:
    variant_info = VARIANTS.get(variant) if variant is not None else None
    CORE.data[KEY_ZEPHYR] = {
        "variant": variant,
        "family": variant_info.family if variant_info is not None else None,
    }


# ---------------------------------------------------------------------------
# Concatenated notation (Renesas RA's own "P<port><2-digit pin>", e.g. "P106")
# ---------------------------------------------------------------------------


def test_concat_pin_accepted_on_renesas_variant() -> None:
    _set_zephyr_variant("RA4M1")
    # P106 -- port 1, pin 06 -- flat GPIO 1 * 16 + 6 = 22.
    assert _validate_gpio_pin("P106") == 22


def test_concat_pin_port_zero_low_pin() -> None:
    _set_zephyr_variant("RA4M1")
    assert _validate_gpio_pin("P000") == 0


def test_concat_pin_highest_valid_pin_in_port() -> None:
    _set_zephyr_variant("RA4M1")
    # RA4M1's gpio_port_width is 16 -- pin 15 is the last valid index in a port.
    assert _validate_gpio_pin("P015") == 15


def test_concat_pin_rejected_when_pin_out_of_range() -> None:
    _set_zephyr_variant("RA4M1")
    with pytest.raises(cv.Invalid, match="only has pins"):
        _validate_gpio_pin("P016")


def test_concat_pin_rejected_on_non_concat_family() -> None:
    # ESP32H2 doesn't use Renesas' concatenated notation.
    _set_zephyr_variant("ESP32H2")
    with pytest.raises(cv.Invalid, match="does not use"):
        _validate_gpio_pin("P106")


def test_concat_pin_rejected_when_no_variant_set() -> None:
    _set_zephyr_variant(None)
    with pytest.raises(cv.Invalid, match="does not use"):
        _validate_gpio_pin("P106")


def test_concat_pin_requires_exactly_two_digit_pin() -> None:
    # A single-digit pin suffix doesn't match the concatenated scheme's fixed
    # 2-digit zero-padding -- falls through to plain int() parsing and fails.
    _set_zephyr_variant("RA4M1")
    with pytest.raises(cv.Invalid, match="Invalid pin number"):
        _validate_gpio_pin("P16")


# ---------------------------------------------------------------------------
# Flat integer / GPIO<N> notation still work identically on a concat-family variant
# ---------------------------------------------------------------------------


def test_flat_int_pin_still_accepted_on_renesas_variant() -> None:
    _set_zephyr_variant("RA4M1")
    assert _validate_gpio_pin(22) == 22


def test_gpio_prefixed_pin_still_accepted_on_renesas_variant() -> None:
    _set_zephyr_variant("RA4M1")
    assert _validate_gpio_pin("GPIO22") == 22


# ---------------------------------------------------------------------------
# pin_summary -- same text as ZephyrGPIOPin::dump_summary()
# ---------------------------------------------------------------------------


@pytest.mark.parametrize(
    ("variant", "num", "expected"),
    [
        ("EFR32MG24", 37, "GPIO37, PC5"),  # lettered ports, 16 pins each
        ("EFR32MG24", 0, "GPIO0, PA0"),
        ("NRF52", 13, "GPIO13, P0.13"),  # port-banked, 32 pins each
        ("NRF52", 45, "GPIO45, P1.13"),
        ("RA4M1", 22, "GPIO22, P106"),  # Renesas: pin zero-padded to 2 digits
        ("RA4M1", 0, "GPIO0, P000"),
        ("ESP32H2", 5, "GPIO5"),  # flat numbering, no port notation
        ("RP2040", 29, "GPIO29"),
    ],
)
def test_pin_summary_uses_the_variants_own_notation(
    variant: str, num: int, expected: str
) -> None:
    assert pin_summary(VARIANTS[variant], num) == expected


def test_pin_summary_falls_back_to_flat_for_port_past_the_last_letter() -> None:
    # EFR32MG24 has ports A-D (4 * 16 pins) -- pin 64 would be a fifth port.
    assert pin_summary(VARIANTS["EFR32MG24"], 64) == "GPIO64"
