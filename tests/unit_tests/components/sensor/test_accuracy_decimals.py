"""Tests for the accuracy_decimals bounds check."""

import pytest

from esphome.components.sensor import validate_accuracy_decimals
import esphome.config_validation as cv


def test_accuracy_decimals_accepts_normal_values() -> None:
    """Values within int8_t range are kept."""
    assert validate_accuracy_decimals(0) == 0
    assert validate_accuracy_decimals(2) == 2
    assert validate_accuracy_decimals(127) == 127


def test_accuracy_decimals_rejects_values_beyond_int8() -> None:
    """The C++ field is int8_t, larger values would silently wrap.

    accuracy_decimals: 200 used to pass validation and become -56 after the
    int8_t conversion, driving garbage into the state formatting.
    """
    with pytest.raises(cv.Invalid):
        validate_accuracy_decimals(200)


def test_accuracy_decimals_rejects_negative_values() -> None:
    """Negative decimal counts have no meaning for state formatting."""
    with pytest.raises(cv.Invalid):
        validate_accuracy_decimals(-3)
