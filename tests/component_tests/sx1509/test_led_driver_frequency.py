"""led_driver_frequency takes one of the nominal frequencies in any frequency form."""

import pytest

from esphome.components.sx1509 import validate_led_driver_frequency
import esphome.config_validation as cv


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("980Hz", "980Hz"),
        ("980 Hz", "980Hz"),
        ("1.96kHz", "1960Hz"),
        ("122HZ", "122Hz"),
    ],
)
def test_frequency_forms(value: str, expected: str) -> None:
    assert validate_led_driver_frequency(value) == expected


def test_other_frequency_rejected() -> None:
    with pytest.raises(cv.Invalid, match="must be one of"):
        validate_led_driver_frequency("1000Hz")
