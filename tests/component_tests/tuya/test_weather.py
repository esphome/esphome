"""Tests for the Tuya weather service configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.tuya import (
    CONF_ON_WEATHER_REQUEST,
    CONF_WEATHER,
    CONFIG_SCHEMA,
    SEND_WEATHER_SCHEMA,
    weather_base_key,
    weather_key_is_string,
    weather_key_takes_sensor,
)
from esphome.const import CONF_UPDATE_INTERVAL


def test_weather_base_key() -> None:
    assert weather_base_key("w.temp") == "w.temp"
    assert weather_base_key("w.temp.1") == "w.temp"
    assert weather_base_key("w.pm25.0") == "w.pm25"
    assert weather_base_key("w.sunRise") == "w.sunRise"
    assert weather_base_key("c.city") == "c.city"
    assert weather_base_key("w.temp.7") is None
    assert weather_base_key("w.unknown") is None
    assert weather_base_key("c.city.1") is None


def test_weather_key_types() -> None:
    assert weather_key_takes_sensor("w.humidity")
    assert weather_key_takes_sensor("w.windSpeed")
    assert weather_key_takes_sensor("w.windSpeed.2")
    assert not weather_key_takes_sensor("w.conditionNum")
    assert weather_key_is_string("w.windSpeed")
    assert weather_key_is_string("w.conditionNum.1")
    assert not weather_key_is_string("w.temp")


def test_weather_sources() -> None:
    config = CONFIG_SCHEMA(
        {CONF_WEATHER: {"w.temp": "temp_id", "w.conditionNum": "condition_id"}}
    )

    weather = config[CONF_WEATHER]
    assert weather["w.temp"].id == "temp_id"
    assert weather["w.conditionNum"].id == "condition_id"
    assert weather[CONF_UPDATE_INTERVAL].total_milliseconds == 30 * 60 * 1000


def test_weather_update_interval_never() -> None:
    config = CONFIG_SCHEMA(
        {CONF_WEATHER: {"w.temp": "temp_id", CONF_UPDATE_INTERVAL: "never"}}
    )

    interval = config[CONF_WEATHER][CONF_UPDATE_INTERVAL].total_milliseconds
    assert interval == cv.SCHEDULER_DONT_RUN


def test_weather_unknown_key() -> None:
    with pytest.raises(cv.Invalid, match="is not a Tuya weather key"):
        CONFIG_SCHEMA({CONF_WEATHER: {"w.rain": "rain_id"}})


def test_weather_needs_a_source() -> None:
    with pytest.raises(cv.Invalid, match="needs at least one Tuya key"):
        CONFIG_SCHEMA({CONF_WEATHER: {CONF_UPDATE_INTERVAL: "30min"}})


def test_weather_request_automation_alone() -> None:
    config = CONFIG_SCHEMA({CONF_WEATHER: {CONF_ON_WEATHER_REQUEST: {"then": []}}})

    assert len(config[CONF_WEATHER][CONF_ON_WEATHER_REQUEST]) == 1


def test_weather_update_interval_minimum() -> None:
    with pytest.raises(cv.Invalid, match="at least 1min"):
        CONFIG_SCHEMA(
            {CONF_WEATHER: {"w.temp": "temp_id", CONF_UPDATE_INTERVAL: "30s"}}
        )


def test_send_weather() -> None:
    config = SEND_WEATHER_SCHEMA({"w.temp": 21, "w.windDir": "NE"})

    assert config["w.temp"] == 21
    assert config["w.windDir"] == "NE"


def test_send_weather_string_for_int_key() -> None:
    with pytest.raises(cv.Invalid):
        SEND_WEATHER_SCHEMA({"w.temp": "warm"})


def test_send_weather_needs_a_key() -> None:
    with pytest.raises(cv.Invalid, match="needs at least one Tuya key"):
        SEND_WEATHER_SCHEMA({})
