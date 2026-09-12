"""Tests for the lis3dh output data rate and operating mode cross-check.

Two of the rates are only meaningful in one operating mode, because the register
code they share means something different in each. Getting the check wrong is
quiet: the part is configured for a rate it cannot deliver in that mode, and the
readings are scaled by the wrong sensitivity rather than anything failing.
"""

from __future__ import annotations

import pytest

from esphome.components.lis3dh.motion import (
    CONF_ACCELEROMETER_ODR,
    CONF_OPERATING_MODE,
    _validate_odr_mode,
)
import esphome.config_validation as cv
from esphome.types import ConfigType


def _config(odr: str, mode: str) -> ConfigType:
    """The two keys the cross-check reads, as validation leaves them.

    cv.enum keeps the option's name, so these are the names rather than the
    values they map to - which is the whole point of the check being written
    this way.
    """
    return {
        CONF_ACCELEROMETER_ODR: cv.enum({odr: None}, upper=True)(odr),
        CONF_OPERATING_MODE: cv.enum({mode: None}, upper=True)(mode),
    }


@pytest.mark.parametrize(
    ("odr", "mode"),
    [
        ("1620HZ", "LOW_POWER"),
        ("1344HZ", "NORMAL"),
        ("1344HZ", "HIGH_RESOLUTION"),
        ("100HZ", "LOW_POWER"),
        ("100HZ", "HIGH_RESOLUTION"),
        ("400HZ", "NORMAL"),
    ],
)
def test_allowed_combinations(odr: str, mode: str) -> None:
    _validate_odr_mode(_config(odr, mode))


@pytest.mark.parametrize("mode", ["NORMAL", "HIGH_RESOLUTION"])
def test_1620hz_needs_low_power(mode: str) -> None:
    """1620Hz exists only in low power mode."""
    with pytest.raises(
        cv.Invalid, match="only available with 'operating_mode: LOW_POWER'"
    ):
        _validate_odr_mode(_config("1620HZ", mode))


def test_1344hz_is_rejected_in_low_power() -> None:
    """The same register code runs at 5376Hz in low power mode."""
    with pytest.raises(
        cv.Invalid, match="not available with 'operating_mode: LOW_POWER'"
    ):
        _validate_odr_mode(_config("1344HZ", "LOW_POWER"))
