"""Tests for the lis3dh config cross-checks and interrupt threshold maths."""

from __future__ import annotations

import pytest

from esphome.components.lis3dh.motion import (
    CONF_ACCELEROMETER_ODR,
    CONF_ACCELEROMETER_RANGE,
    CONF_INTERRUPT,
    CONF_OPERATING_MODE,
    CONF_THRESHOLD,
    _threshold_register,
    _validate_odr_mode,
    _validate_threshold,
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


def test_1620hz_low_power_is_accepted() -> None:
    """The one valid 1620Hz combination passes through unchanged."""
    config = {CONF_ACCELEROMETER_ODR: "1620HZ", CONF_OPERATING_MODE: "LOW_POWER"}
    assert _validate_odr_mode(config) is config


@pytest.mark.parametrize(
    ("threshold_g", "range_key", "expected"),
    [
        (0.0, "2G", 1),
        (0.05, "16G", 1),
        (1.1, "2G", 69),
        (0.2, "2G", 13),
        (2.032, "2G", 127),
        (23.622, "16G", 127),
    ],
)
def test_threshold_register(threshold_g: float, range_key: str, expected: int) -> None:
    """INT1_THS is the nearest step and never zero."""
    assert _threshold_register(threshold_g, range_key) == expected


@pytest.mark.parametrize(
    ("threshold_g", "range_key"),
    [(2.04, "2G"), (3.0, "2G"), (4.07, "4G"), (23.7, "16G")],
)
def test_threshold_above_range_is_rejected(threshold_g: float, range_key: str) -> None:
    """A threshold the range cannot represent would never fire, so it fails validation."""
    config = {
        CONF_ACCELEROMETER_RANGE: range_key,
        CONF_INTERRUPT: {CONF_THRESHOLD: threshold_g},
    }
    with pytest.raises(cv.Invalid, match="at most"):
        _validate_threshold(config)


def test_threshold_within_range_is_accepted() -> None:
    """The largest representable threshold is still valid."""
    config = {CONF_ACCELEROMETER_RANGE: "2G", CONF_INTERRUPT: {CONF_THRESHOLD: 2.032}}
    assert _validate_threshold(config) is config
