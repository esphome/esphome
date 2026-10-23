"""Validate RX8111 alarm and timer options before generating firmware."""

import pytest

from esphome.components.rx8111.time import (
    _ALARM_DURATION,
    ALARM_FIELD_UNSET,
    CONF_ALARM_DAY,
    CONF_ALARM_ENABLED,
    CONF_ALARM_WEEKDAY,
    CONF_BATTERY_BACKUP,
    CONF_BATTERY_CHARGING,
    CONF_DAY,
    CONF_ON_ALARM,
    CONF_ON_TIMER,
    CONF_SECOND,
    CONF_TIMER_COUNT,
    CONF_TIMER_ENABLED,
    CONF_WEEKDAY,
    FOUT_FREQUENCIES,
    _validate_alarm_config,
    _validate_alarm_options,
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
        _validate_alarm_options({CONF_ALARM_DAY: 1, CONF_ALARM_WEEKDAY: 2})


@pytest.mark.parametrize("duration", ["0s", "28d", "29d"])
def test_relative_alarm_rejects_out_of_range_duration(duration: str) -> None:
    with pytest.raises(cv.Invalid):
        _ALARM_DURATION(duration)


def test_relative_alarm_accepts_mixed_units() -> None:
    assert _ALARM_DURATION({"days": 1, "minutes": 1}).total_seconds == 86460


@pytest.mark.parametrize(
    "config",
    [
        {CONF_TIMER_ENABLED: True},
        {CONF_ON_TIMER: [{}]},
        {CONF_TIMER_ENABLED: False, CONF_ON_TIMER: [{}]},
    ],
)
def test_timer_requires_count(config: dict) -> None:
    with pytest.raises(cv.Invalid, match="timer_count"):
        _validate_alarm_config(config)


def test_timer_count_allows_explicit_disabled_configuration() -> None:
    config = {CONF_TIMER_COUNT: 0xFFFFFF, CONF_TIMER_ENABLED: False}
    assert _validate_alarm_config(config) == config


def test_on_alarm_requires_enabled_alarm() -> None:
    with pytest.raises(cv.Invalid, match="on_alarm"):
        _validate_alarm_config({CONF_ON_ALARM: [{}]})
    assert _validate_alarm_config({CONF_ON_ALARM: [{}], CONF_ALARM_ENABLED: True})


def test_charging_requires_explicit_backup() -> None:
    for config in (
        {CONF_BATTERY_CHARGING: True},
        {CONF_BATTERY_BACKUP: False, CONF_BATTERY_CHARGING: True},
    ):
        with pytest.raises(cv.Invalid, match="battery_backup"):
            _validate_alarm_config(config)
    assert _validate_alarm_config(
        {CONF_BATTERY_BACKUP: True, CONF_BATTERY_CHARGING: True}
    )


@pytest.mark.parametrize("value", ["32768hz", "1024hz", "1hz", "off"])
def test_fout_frequencies(value: str) -> None:
    assert (
        cv.enum(FOUT_FREQUENCIES, lower=True)(value).enum_value
        == FOUT_FREQUENCIES[value]
    )


def test_invalid_fout_frequency() -> None:
    with pytest.raises(cv.Invalid):
        cv.enum(FOUT_FREQUENCIES, lower=True)("100hz")


def test_event_configuration_requires_consumer() -> None:
    from esphome.components.rx8111.time import (
        CONF_EVENT_ENABLED,
        CONF_EVENT_LEVEL,
        CONF_EVIN_FILTER,
        CONF_ON_EVENT,
    )

    for config in (
        {CONF_ON_EVENT: [{}]},
        {CONF_EVENT_LEVEL: 0},
        {CONF_EVIN_FILTER: 2},
    ):
        with pytest.raises(cv.Invalid):
            _validate_alarm_config(config)
    assert _validate_alarm_config({CONF_EVENT_ENABLED: True, CONF_ON_EVENT: [{}]})
    assert _validate_alarm_config({CONF_EVENT_ENABLED: False})


@pytest.mark.parametrize("option", ["event_timestamp", "timestamp_record_mode"])
def test_timestamp_options_are_not_in_time_platform(option: str) -> None:
    from esphome.components.rx8111.time import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({option: "latest"})
