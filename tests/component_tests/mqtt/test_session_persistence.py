"""Tests for sizing the MQTT persisted subscription table."""

from __future__ import annotations

from collections.abc import Callable
import logging
from typing import Any

import pytest

from esphome.components import mqtt
from esphome.components.mqtt import CONF_MAX_PERSISTED_SUBSCRIPTIONS, _final_validate
from esphome.const import (
    CONF_CLEAN_SESSION,
    CONF_COMMAND_TOPIC,
    CONF_DISCOVER_IP,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_ON_JSON_MESSAGE,
    CONF_ON_MESSAGE,
    CONF_PLATFORM,
)
from esphome.core import ID
from esphome.cpp_generator import MockObjClass
from esphome.types import ConfigType


def _entity(mqtt_class: MockObjClass, **kwargs: Any) -> ConfigType:
    return {CONF_MQTT_ID: ID(None, is_declaration=True, type=mqtt_class), **kwargs}


def _mqtt_config(clean_session: Any = "FLASH", **kwargs: Any) -> ConfigType:
    return {CONF_CLEAN_SESSION: clean_session, CONF_DISCOVER_IP: False, **kwargs}


def test_not_sized_without_tracking(
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_component_config("switch", [_entity(mqtt.MQTTSwitchComponent)])
    for clean_session in (True, False):
        config = _final_validate(_mqtt_config(clean_session))
        assert CONF_MAX_PERSISTED_SUBSCRIPTIONS not in config


def test_client_subscriptions_counted() -> None:
    config = _final_validate(
        _mqtt_config(
            "RTC",
            **{
                CONF_DISCOVER_IP: True,
                CONF_ON_MESSAGE: [{}, {}],
                CONF_ON_JSON_MESSAGE: [{}],
            },
        )
    )
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 5


def test_at_least_one_slot() -> None:
    config = _final_validate(_mqtt_config())
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 1


def test_entities_counted_per_class(
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_component_config(
        "cover", [_entity(mqtt.MQTTCoverComponent), _entity(mqtt.MQTTCoverComponent)]
    )
    set_component_config("climate", [_entity(mqtt.MQTTClimateComponent)])
    # Publish only
    set_component_config("sensor", [_entity(mqtt.MQTTSensorComponent)])
    config = _final_validate(_mqtt_config())
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 2 * 3 + 7


def test_nested_entities_counted(
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_component_config(
        "number",
        [
            {
                CONF_PLATFORM: "ld2410",
                "timeout": _entity(mqtt.MQTTNumberComponent),
                "g0": {
                    "move_threshold": _entity(mqtt.MQTTNumberComponent),
                    "still_threshold": _entity(mqtt.MQTTNumberComponent),
                },
            }
        ],
    )
    set_component_config(
        "sprinkler",
        [{"valves": [{"enable_switch": _entity(mqtt.MQTTSwitchComponent)}]}],
    )
    config = _final_validate(_mqtt_config())
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 4


def test_internal_entities(
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_component_config(
        "switch",
        [
            _entity(mqtt.MQTTSwitchComponent, **{CONF_INTERNAL: True}),
            # A custom topic keeps the MQTT component active
            _entity(
                mqtt.MQTTSwitchComponent,
                **{CONF_INTERNAL: True, CONF_COMMAND_TOPIC: "a/b"},
            ),
        ],
    )
    # mqtt_subscribe subscribes even when internal
    set_component_config(
        "sensor",
        [
            _entity(
                mqtt.MQTTSensorComponent,
                **{CONF_PLATFORM: "mqtt_subscribe", CONF_INTERNAL: True},
            )
        ],
    )
    config = _final_validate(_mqtt_config())
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 2


def test_explicit_value_kept_and_warns_when_too_low(
    set_component_config: Callable[[str, Any], None],
    caplog: pytest.LogCaptureFixture,
) -> None:
    set_component_config("switch", [_entity(mqtt.MQTTSwitchComponent)] * 3)
    with caplog.at_level(logging.WARNING):
        config = _final_validate(_mqtt_config(**{CONF_MAX_PERSISTED_SUBSCRIPTIONS: 2}))
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 2
    assert "at least 3" in caplog.text


def test_explicit_value_large_enough_does_not_warn(
    set_component_config: Callable[[str, Any], None],
    caplog: pytest.LogCaptureFixture,
) -> None:
    set_component_config("switch", [_entity(mqtt.MQTTSwitchComponent)])
    with caplog.at_level(logging.WARNING):
        config = _final_validate(_mqtt_config(**{CONF_MAX_PERSISTED_SUBSCRIPTIONS: 10}))
    assert config[CONF_MAX_PERSISTED_SUBSCRIPTIONS] == 10
    assert not caplog.text
