"""Tests for the lis3dh output data rate and operating mode cross-check."""

from __future__ import annotations

import pytest

from esphome.components.lis3dh.motion import (
    CONF_ACCELEROMETER_ODR,
    CONF_OPERATING_MODE,
    _validate_odr_mode,
)
import esphome.config_validation as cv


@pytest.mark.parametrize("mode", ["NORMAL", "HIGH_RESOLUTION"])
def test_1620hz_needs_low_power(mode: str) -> None:
    """1620Hz exists only in low power mode."""
    config = {CONF_ACCELEROMETER_ODR: "1620HZ", CONF_OPERATING_MODE: mode}
    with pytest.raises(cv.Invalid, match="only available with"):
        _validate_odr_mode(config)


def test_1344hz_is_rejected_in_low_power() -> None:
    """The 1344Hz code runs at 5376Hz in low power mode."""
    config = {CONF_ACCELEROMETER_ODR: "1344HZ", CONF_OPERATING_MODE: "LOW_POWER"}
    with pytest.raises(cv.Invalid, match="not available with"):
        _validate_odr_mode(config)
