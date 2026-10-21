"""Validate RX8111 alarm and timer options before generating firmware."""

import pytest

from esphome.components.rx8111.time import (
    ALARM_FIELD_UNSET,
    CONF_ALARM_DAY,
    CONF_ALARM_WEEKDAY,
    CONF_DAY,
    CONF_DAYS,
    CONF_ON_TIMER,
    CONF_SECOND,
    CONF_SECONDS,
    CONF_TIMER_COUNT,
    CONF_TIMER_ENABLED,
    CONF_WEEKDAY,
    _validate_alarm_config,
    _validate_alarm_duration,
    _validate_set_alarm,
)
import esphome.config_validation as cv


@pytest.mark.parametrize(
    "config",
    [{}, {CONF_SECOND: 0, CONF_WEEKDAY: 1, CONF_DAY: 1}],
)
def test_set_alarm_rejects_empty_or_conflicting_fields(config: dict) -> None:
    with pytest.raises(cv.Invalid):
        _validate_set_alarm(config)


def test_set_alarm_marks_missing_fields_unset() -> None:
    config = _validate_set_alarm({CONF_SECOND: 5})
    assert config[CONF_SECOND] == 5
    assert config[CONF_DAY] == ALARM_FIELD_UNSET


def test_fixed_alarm_rejects_day_and_weekday() -> None:
    with pytest.raises(cv.Invalid):
        _validate_alarm_config({CONF_ALARM_DAY: 1, CONF_ALARM_WEEKDAY: 2})


@pytest.mark.parametrize("duration", [0, 28 * 86400, 29 * 86400])
def test_relative_alarm_rejects_out_of_range_duration(duration: int) -> None:
    with pytest.raises(cv.Invalid):
        _validate_alarm_duration({CONF_SECONDS: duration})


def test_relative_alarm_combines_units() -> None:
    config = _validate_alarm_duration({CONF_DAYS: 1, CONF_SECONDS: 60})
    assert config[CONF_SECONDS] == 86460


@pytest.mark.parametrize(
    "config",
    [{CONF_TIMER_ENABLED: True}, {CONF_ON_TIMER: [{}]}],
)
def test_timer_requires_count(config: dict) -> None:
    with pytest.raises(cv.Invalid, match="timer_count"):
        _validate_alarm_config(config)


def test_timer_count_allows_explicit_disabled_configuration() -> None:
    config = {CONF_TIMER_COUNT: 0xFFFFFF, CONF_TIMER_ENABLED: False}
    assert _validate_alarm_config(config) == config
