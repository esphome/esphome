"""Tests for the shared BLE tracker scan parameter validation."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.bk72xx_ble_tracker import (
    SCAN_PARAMETERS_SCHEMA as BK72XX_SCHEMA,
)
from esphome.components.ble_device_base import to_ble_units
from esphome.components.esp32_ble_tracker import SCAN_PARAMETERS_SCHEMA as ESP32_SCHEMA
from esphome.components.ln882h_ble_tracker import (
    SCAN_PARAMETERS_SCHEMA as LN882H_SCHEMA,
)
from esphome.components.rp2_ble_tracker import SCAN_PARAMETERS_SCHEMA as RP2_SCHEMA


def _validate(**kwargs: str | bool) -> dict:
    """Run a scan_parameters config through the bk72xx tracker's real schema."""
    return BK72XX_SCHEMA(kwargs)


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


# --- the real per-chip schemas ---


def test_bk72xx_defaults_are_valid() -> None:
    """bk72xx pins the BK reference rate — 100 ms interval, shared 30 ms window —
    and exposes active (default on, like every active-capable tracker)."""
    config = _validate()
    assert to_ble_units(config["interval"]) == 160
    assert to_ble_units(config["window"]) == 48
    assert config["active"] is True


def test_esp32_defaults_are_valid() -> None:
    """esp32 pins the ESP-IDF reference rate and exposes active (default on).

    Without wifi loaded, the conditional window default falls back to the
    historical 30 ms; the wifi-aware resolution is covered by the
    esp32_ble_tracker component tests.
    """
    config = ESP32_SCHEMA({})
    assert to_ble_units(config["interval"]) == 512
    assert to_ble_units(config["window"]) == 48
    assert config["active"] is True


def test_rp2_defaults_are_valid() -> None:
    """rp2 pins 100 ms interval / 30 ms window — a 30 % duty cycle leaving the
    shared CYW43 radio mostly free for WiFi — and exposes active (default on)."""
    config = RP2_SCHEMA({})
    assert to_ble_units(config["interval"]) == 160
    assert to_ble_units(config["window"]) == 48
    assert config["active"] is True


def test_ln882h_defaults_are_valid() -> None:
    """ln882h pins the LN SDK reference rate — 100 ms interval / 50 ms window
    (50 % duty) — and exposes active (default on)."""
    config = LN882H_SCHEMA({})
    assert to_ble_units(config["interval"]) == 160
    assert to_ble_units(config["window"]) == 80
    assert config["active"] is True


def test_esp32_active_can_disable() -> None:
    config = ESP32_SCHEMA({"active": False})
    assert config["active"] is False


def test_bk72xx_active_can_disable() -> None:
    config = _validate(active=False)
    assert config["active"] is False


# --- accepted configurations ---


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


def test_duration_equal_to_three_intervals_accepted() -> None:
    """The three-interval floor is inclusive, mirroring the ceilings above."""
    _validate(duration="3s", interval="1s", window="500ms")


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
    """3000us/2500us both floor to 4 units — a hidden 100 % duty cycle."""
    with pytest.raises(cv.Invalid, match="both truncate to 4 x 0.625 ms"):
        _validate(interval="3000us", window="2500us")


def test_duration_shorter_than_three_intervals_rejected() -> None:
    with pytest.raises(cv.Invalid, match="must cover at least three scan intervals"):
        _validate(duration="1s", interval="500ms", window="100ms")
