"""Tests for the ld2450 link timeout option and the connected binary sensor."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.const import CONF_CONNECTED
from esphome.components.ld2450 import CONF_LINK_TIMEOUT, CONFIG_SCHEMA
from esphome.components.ld2450.binary_sensor import (
    CONFIG_SCHEMA as BINARY_SENSOR_CONFIG_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICE_CLASS,
    CONF_ENTITY_CATEGORY,
    CONF_FILTERS,
    CONF_HAS_TARGET,
    CONF_NAME,
    DEVICE_CLASS_CONNECTIVITY,
    ENTITY_CATEGORY_DIAGNOSTIC,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable


def test_link_timeout_defaults_to_5_seconds(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA({})

    assert config[CONF_LINK_TIMEOUT].total_milliseconds == 5000


@pytest.mark.parametrize(
    ("value", "milliseconds"),
    [("1s", 1000), ("30s", 30_000), ("2min", 120_000), ("1h", 3_600_000)],
)
def test_link_timeout_accepts_one_second_to_one_hour(
    set_core_config: SetCoreConfigCallable, value: str, milliseconds: int
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = CONFIG_SCHEMA({CONF_LINK_TIMEOUT: value})

    assert config[CONF_LINK_TIMEOUT].total_milliseconds == milliseconds


@pytest.mark.parametrize(
    ("value", "message"),
    [
        ("0.5s", r"must be at least 1s"),
        ("0s", r"must be at least 1s"),
        ("999ms", r"must be at least 1s"),
        ("2h", r"must be at most 1h"),
        ("3601s", r"must be at most 1h"),
    ],
)
def test_link_timeout_outside_the_range_is_rejected(
    set_core_config: SetCoreConfigCallable, value: str, message: str
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match=message):
        CONFIG_SCHEMA({CONF_LINK_TIMEOUT: value})


def test_link_timeout_reaches_the_hub_in_milliseconds(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("link_connected.yaml"))

    assert "radar->set_link_timeout(30000);" in main_cpp


def test_default_link_timeout_reaches_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("link_default.yaml"))

    assert "radar->set_link_timeout(5000);" in main_cpp
    assert "set_connected_binary_sensor" not in main_cpp


def test_connected_sensor_is_wired_to_the_hub(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("link_connected.yaml"))

    assert "radar->set_connected_binary_sensor(" in main_cpp
    assert "radar->set_target_binary_sensor(" in main_cpp


def test_connected_sensor_is_a_diagnostic_connectivity_sensor_without_filters(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = cv.Schema(BINARY_SENSOR_CONFIG_SCHEMA)(
        {CONF_CONNECTED: {CONF_NAME: "Radar Link"}}
    )

    connected = config[CONF_CONNECTED]
    assert connected[CONF_DEVICE_CLASS] == DEVICE_CLASS_CONNECTIVITY
    assert connected[CONF_ENTITY_CATEGORY] == ENTITY_CATEGORY_DIAGNOSTIC
    assert CONF_FILTERS not in connected


def test_presence_sensors_keep_their_default_settle_filter(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    config = cv.Schema(BINARY_SENSOR_CONFIG_SCHEMA)({CONF_HAS_TARGET: {CONF_NAME: "x"}})

    assert CONF_FILTERS in config[CONF_HAS_TARGET]
