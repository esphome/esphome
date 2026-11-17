"""Tests for the ds3231 hub and alarm action validation."""

import pytest

from esphome import config_validation as cv
from esphome.components import ds3231
from esphome.components.ds3231 import (
    ALARM_1_MODES,
    ALARM_2_MODES,
    CONF_DAY_OF_MONTH,
    CONF_DAY_OF_WEEK,
    SQUARE_WAVE_FREQUENCIES,
    SquareWaveFrequency,
    validate_square_wave_frequency,
)
from esphome.const import CONF_MODE


@pytest.mark.parametrize(
    ("value", "frequency"),
    [
        ("1Hz", SquareWaveFrequency.SQUARE_WAVE_1HZ),
        ("1024Hz", SquareWaveFrequency.SQUARE_WAVE_1024HZ),
        ("1.024kHz", SquareWaveFrequency.SQUARE_WAVE_1024HZ),
        ("4096 Hz", SquareWaveFrequency.SQUARE_WAVE_4096HZ),
        (8192, SquareWaveFrequency.SQUARE_WAVE_8192HZ),
    ],
)
def test_square_wave_frequency_accepts_every_spelling(
    value: str | int, frequency: object
) -> None:
    assert validate_square_wave_frequency(value) == frequency


def test_square_wave_frequency_rejects_other_rates() -> None:
    with pytest.raises(cv.Invalid, match="valid options are"):
        validate_square_wave_frequency("2048Hz")
    assert list(SQUARE_WAVE_FREQUENCIES) == [1, 1024, 4096, 8192]


def test_alarm_modes_map_to_their_enumerators() -> None:
    assert str(ALARM_1_MODES["every_second"]) == "ds3231::ALARM_MODE_EVERY_SECOND"
    assert str(ALARM_1_MODES["weekly"]) == "ds3231::ALARM_MODE_WEEKLY"
    assert "every_second" not in ALARM_2_MODES


def _alarm_1_schema() -> cv.Schema:
    return ds3231._alarm_schema(ALARM_1_MODES, with_second=True)


def test_monthly_alarm_needs_a_day_of_month() -> None:
    with pytest.raises(cv.Invalid, match="'day_of_month' is required in monthly mode"):
        _alarm_1_schema()({CONF_MODE: "monthly"})


def test_weekly_alarm_needs_a_day_of_week() -> None:
    with pytest.raises(cv.Invalid, match="'day_of_week' is required in weekly mode"):
        _alarm_1_schema()({CONF_MODE: "weekly"})


def test_daily_alarm_fills_the_unused_days() -> None:
    config = _alarm_1_schema()({CONF_MODE: "daily"})
    assert config[CONF_DAY_OF_MONTH] == 1
    assert config[CONF_DAY_OF_WEEK] == 1


def test_alarm_2_rejects_every_second() -> None:
    schema = ds3231._alarm_schema(ALARM_2_MODES, with_second=False)
    with pytest.raises(cv.Invalid, match="Unknown value 'every_second'"):
        schema({CONF_MODE: "every_second"})
