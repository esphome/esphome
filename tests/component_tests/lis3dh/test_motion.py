"""Tests for the lis3dh config cross-checks and interrupt and tap threshold maths."""

from __future__ import annotations

import logging

import pytest

from esphome.components.lis3dh.motion import (
    CONF_ACCELEROMETER_ODR,
    CONF_ACCELEROMETER_RANGE,
    CONF_DURATION,
    CONF_HIGH_PASS_FILTER,
    CONF_INTERRUPT,
    CONF_INTERRUPT_PIN,
    CONF_LATENCY,
    CONF_ON_DOUBLE_TAP,
    CONF_ON_TAP,
    CONF_OPERATING_MODE,
    CONF_PIN,
    CONF_TAP,
    CONF_THRESHOLD,
    CONF_TIME_LIMIT,
    CONF_WINDOW,
    _duration_register,
    _threshold_register,
    _validate_interrupt,
    _validate_odr_mode,
    _validate_tap,
)
import esphome.config_validation as cv
from esphome.core import TimePeriod


def _interrupt_config(
    range_key: str,
    odr_key: str,
    threshold_g: float,
    duration_ms: int,
    high_pass: bool = True,
) -> dict:
    return {
        CONF_ACCELEROMETER_RANGE: range_key,
        CONF_ACCELEROMETER_ODR: odr_key,
        CONF_INTERRUPT: {
            CONF_THRESHOLD: threshold_g,
            CONF_DURATION: TimePeriod(milliseconds=duration_ms),
            CONF_HIGH_PASS_FILTER: high_pass,
        },
    }


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
    config = _interrupt_config(range_key, "100HZ", threshold_g, 0)
    with pytest.raises(cv.Invalid, match="'threshold' can be at most"):
        _validate_interrupt(config)


def test_threshold_within_range_is_accepted() -> None:
    """The largest representable threshold is still valid."""
    config = _interrupt_config("2G", "100HZ", 2.032, 0)
    assert _validate_interrupt(config) is config


@pytest.mark.parametrize(
    ("duration_ms", "odr_key", "expected"),
    [
        (0, "100HZ", 0),
        (20, "100HZ", 2),
        (1, "1620HZ", 2),
        (10000, "1HZ", 10),
        (4, "100HZ", 0),
    ],
)
def test_duration_register(duration_ms: int, odr_key: str, expected: int) -> None:
    """INT1_DURATION is the nearest number of samples at the data rate."""
    assert _duration_register(duration_ms, odr_key) == expected


@pytest.mark.parametrize(
    ("duration_ms", "odr_key"), [(1280, "100HZ"), (128000, "1HZ"), (79, "1620HZ")]
)
def test_duration_above_register_is_rejected(duration_ms: int, odr_key: str) -> None:
    """A duration that needs more than 127 samples fails validation."""
    config = _interrupt_config("2G", odr_key, 1.1, duration_ms)
    with pytest.raises(cv.Invalid, match="'duration' can be at most"):
        _validate_interrupt(config)


def test_duration_at_register_limit_is_accepted() -> None:
    """127 samples is the largest INT1_DURATION value."""
    config = _interrupt_config("2G", "100HZ", 1.1, 1270)
    assert _validate_interrupt(config) is config


def test_sub_gravity_threshold_without_filter_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Gravity alone can hold the pad asserted below 1g unless the high-pass filter is on."""
    config = _interrupt_config("2G", "100HZ", 0.5, 0, high_pass=False)
    with caplog.at_level(logging.WARNING):
        assert _validate_interrupt(config) is config
    assert "high_pass_filter: true" in caplog.text


@pytest.mark.parametrize(("threshold_g", "high_pass"), [(1.1, False), (0.5, True)])
def test_threshold_without_gravity_problem_is_silent(
    caplog: pytest.LogCaptureFixture, threshold_g: float, high_pass: bool
) -> None:
    """Above 1g, or with the filter on, no warning is logged."""
    config = _interrupt_config("2G", "100HZ", threshold_g, 0, high_pass=high_pass)
    with caplog.at_level(logging.WARNING):
        _validate_interrupt(config)
    assert caplog.text == ""


def _tap_config(
    odr_key: str = "400HZ",
    threshold_g: float = 0.6,
    time_limit_ms: int = 25,
    latency_ms: int = 50,
    window_ms: int = 250,
    triggers: tuple[str, ...] = (CONF_ON_TAP,),
    **tap_extra: object,
) -> dict:
    tap = {
        CONF_THRESHOLD: threshold_g,
        CONF_TIME_LIMIT: TimePeriod(milliseconds=time_limit_ms),
        CONF_LATENCY: TimePeriod(milliseconds=latency_ms),
        CONF_WINDOW: TimePeriod(milliseconds=window_ms),
        CONF_PIN: "INT1",
        **{trigger: [] for trigger in triggers},
        **tap_extra,
    }
    return {
        CONF_ACCELEROMETER_RANGE: "2G",
        CONF_ACCELEROMETER_ODR: odr_key,
        CONF_TAP: tap,
    }


def test_tap_defaults_are_accepted() -> None:
    """The schema defaults at 400Hz pass validation unchanged."""
    config = _tap_config(triggers=(CONF_ON_TAP, CONF_ON_DOUBLE_TAP))
    assert _validate_tap(config) is config


def test_tap_threshold_above_range_is_rejected() -> None:
    """CLICK_THS holds seven bits, the same limit as the motion interrupt."""
    config = _tap_config(threshold_g=2.1)
    with pytest.raises(cv.Invalid, match="'threshold' can be at most 2.032g"):
        _validate_tap(config)


def test_tap_time_below_one_sample_is_rejected() -> None:
    """A time that rounds to zero samples would disable the detector."""
    config = _tap_config(time_limit_ms=1)
    with pytest.raises(cv.Invalid, match="'time_limit' must be at least 2ms"):
        _validate_tap(config)


@pytest.mark.parametrize(
    ("key", "kwargs", "max_ms"),
    [
        (CONF_TIME_LIMIT, {"time_limit_ms": 400}, 317),
        (CONF_LATENCY, {"latency_ms": 700}, 637),
        (CONF_WINDOW, {"window_ms": 700}, 637),
    ],
)
def test_tap_time_above_register_is_rejected(
    key: str, kwargs: dict, max_ms: int
) -> None:
    """TIME_LIMIT holds seven bits and TIME_LATENCY / TIME_WINDOW eight."""
    config = _tap_config(triggers=(CONF_ON_DOUBLE_TAP,), **kwargs)
    with pytest.raises(cv.Invalid, match=f"'{key}' can be at most {max_ms}ms"):
        _validate_tap(config)


def test_double_tap_times_are_ignored_without_double_tap() -> None:
    """The latency and window times only matter for double taps."""
    config = _tap_config(latency_ms=700, window_ms=700)
    assert _validate_tap(config) is config


def test_interrupt_pin_with_both_triggers_is_rejected() -> None:
    """With both enabled the chip raises the interrupt only for double taps."""
    config = _tap_config(
        triggers=(CONF_ON_TAP, CONF_ON_DOUBLE_TAP), **{CONF_INTERRUPT_PIN: 15}
    )
    with pytest.raises(cv.Invalid, match="can't be used with both"):
        _validate_tap(config)


@pytest.mark.parametrize("trigger", [CONF_ON_TAP, CONF_ON_DOUBLE_TAP])
def test_interrupt_pin_with_one_trigger_is_accepted(trigger: str) -> None:
    """Either trigger alone can be read from the interrupt pin."""
    config = _tap_config(triggers=(trigger,), **{CONF_INTERRUPT_PIN: 15})
    assert _validate_tap(config) is config


def test_tap_on_motion_interrupt_pad_is_rejected() -> None:
    """Tap and motion cannot share a pad when the tap pad is watched."""
    config = _tap_config(**{CONF_INTERRUPT_PIN: 15})
    config[CONF_INTERRUPT] = {CONF_PIN: "INT1"}
    with pytest.raises(cv.Invalid, match="already uses INT1"):
        _validate_tap(config)


def test_polled_tap_ignores_motion_interrupt_pad() -> None:
    """Without interrupt_pin the tap pad is unused, so it may match the motion pad."""
    config = _tap_config()
    config[CONF_INTERRUPT] = {CONF_PIN: "INT1"}
    assert _validate_tap(config) is config


def test_tap_below_400hz_warns(caplog: pytest.LogCaptureFixture) -> None:
    """Most taps fall between samples at low data rates."""
    config = _tap_config(odr_key="100HZ")
    with caplog.at_level(logging.WARNING):
        assert _validate_tap(config) is config
    assert "400HZ" in caplog.text


def test_tap_at_400hz_is_silent(caplog: pytest.LogCaptureFixture) -> None:
    """At 400Hz no warning is logged."""
    with caplog.at_level(logging.WARNING):
        _validate_tap(_tap_config())
    assert caplog.text == ""
