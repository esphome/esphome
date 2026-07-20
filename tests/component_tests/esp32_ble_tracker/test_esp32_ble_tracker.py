"""Tests for esp32_ble_tracker configuration helpers."""

import logging

import pytest

from esphome.components.esp32_ble_tracker import (
    CONF_DURATION,
    CONF_INTERVAL,
    CONF_WINDOW,
    validate_scan_parameters,
)
import esphome.config_validation as cv


def _scan_params(interval_ms: int, window_ms: int, duration_s: int = 300) -> dict:
    """Build a scan_parameters config the way the schema would produce it."""
    return {
        CONF_DURATION: cv.positive_time_period_seconds(f"{duration_s}s"),
        CONF_INTERVAL: cv.positive_time_period_milliseconds(f"{interval_ms}ms"),
        CONF_WINDOW: cv.positive_time_period_milliseconds(f"{window_ms}ms"),
    }


def test_scan_window_smaller_than_interval_is_silent(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A scan window below the interval leaves idle time and must not warn."""
    with caplog.at_level(logging.WARNING):
        validate_scan_parameters(_scan_params(interval_ms=320, window_ms=30))
    assert "scans continuously" not in caplog.text


def test_scan_window_equal_to_interval_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Window == interval means continuous scanning and must warn the user."""
    with caplog.at_level(logging.WARNING):
        validate_scan_parameters(_scan_params(interval_ms=1100, window_ms=1100))
    assert "scans continuously" in caplog.text


def test_scan_window_greater_than_interval_is_invalid() -> None:
    """Window > interval is physically impossible and must be rejected."""
    with pytest.raises(cv.Invalid, match="needs to be smaller"):
        validate_scan_parameters(_scan_params(interval_ms=320, window_ms=400))
