"""Tests for bk72xx_ble_tracker scan parameter validation."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.bk72xx_ble_tracker import SCAN_PARAMETERS_SCHEMA, to_ble_units


def _validate(**kwargs: str) -> dict:
    """Run a scan_parameters config through the schema, applying defaults."""
    return SCAN_PARAMETERS_SCHEMA(dict(kwargs))


# --- to_ble_units ---


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("2500us", 4),  # controller minimum, 2.5 ms
        ("30ms", 48),
        ("100ms", 160),
        ("10240ms", 16384),  # controller maximum, 0x4000
    ],
)
def test_to_ble_units_converts_to_controller_units(value: str, expected: int) -> None:
    """A time is converted to whole 0.625 ms units."""
    assert to_ble_units(cv.positive_time_period(value)) == expected


def test_to_ble_units_truncates() -> None:
    """Sub-unit remainders are dropped, which is what makes collapse possible."""
    assert to_ble_units(cv.positive_time_period("3000us")) == 4
    assert to_ble_units(cv.positive_time_period("2500us")) == 4


# --- accepted configurations ---


def test_defaults_are_valid() -> None:
    """The documented default 100 ms / 30 ms pair validates."""
    config = _validate()
    assert to_ble_units(config["interval"]) == 160
    assert to_ble_units(config["window"]) == 48


def test_minimum_separation_accepted() -> None:
    """Values one unit apart at the 2.5 ms floor are honest, not collapsed."""
    config = _validate(interval="5000us", window="2500us")
    assert to_ble_units(config["interval"]) == 8
    assert to_ble_units(config["window"]) == 4


def test_maximum_interval_accepted() -> None:
    """The documented 10240 ms ceiling is inclusive, and maps to 0x4000.

    Pins the ceiling from the accept side, mirroring the 2.5 ms floor above: the
    reject cases alone would let the bound silently become exclusive.
    """
    config = _validate(interval="10240ms", window="30ms")
    assert to_ble_units(config["interval"]) == 16384


def test_maximum_window_accepted() -> None:
    """The ceiling applies to the window too, and is likewise inclusive."""
    config = _validate(interval="10240ms", window="10240ms")
    assert to_ble_units(config["window"]) == 16384


def test_window_equal_to_interval_accepted() -> None:
    """A deliberate 100 % duty cycle is allowed; only an accidental one is not."""
    config = _validate(interval="100ms", window="100ms")
    assert to_ble_units(config["interval"]) == to_ble_units(config["window"])


# --- rejected configurations ---


def test_window_larger_than_interval_rejected() -> None:
    with pytest.raises(cv.Invalid, match="needs to be smaller than scan interval"):
        _validate(interval="30ms", window="100ms")


@pytest.mark.parametrize(
    ("interval", "window", "offender"),
    [
        ("2ms", "1ms", "interval"),  # below the 2.5 ms controller floor
        ("20s", "1s", "interval"),  # above the 10240 ms controller ceiling
        ("100ms", "1ms", "window"),  # window below the floor
    ],
)
def test_out_of_range_rejected(interval: str, window: str, offender: str) -> None:
    """Values the controller cannot represent are rejected, not silently wrapped."""
    with pytest.raises(
        cv.Invalid, match=f"Scan {offender} .* must be between 2.5 ms and 10240 ms"
    ):
        _validate(interval=interval, window=window)


def test_unit_collapse_rejected() -> None:
    """Regression: 3000us/2500us both floor to 4 units — a hidden 100 % duty cycle.

    This is the configuration that previously validated and programmed the radio
    permanently on despite asking for roughly 83 %.
    """
    with pytest.raises(cv.Invalid, match="both round to 4 x 0.625 ms"):
        _validate(interval="3000us", window="2500us")


def test_duration_shorter_than_three_intervals_rejected() -> None:
    with pytest.raises(cv.Invalid, match="must cover at least three scan intervals"):
        _validate(duration="1s", interval="500ms", window="100ms")
