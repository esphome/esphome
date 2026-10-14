"""Tests for the mqtt check that subscribed topics fit the AsyncMqttClient limit."""

import pytest

from esphome.components import mqtt
from esphome.components.mqtt import (
    _MQTT_COMMAND_TOPICS,
    _MQTT_WITHOUT_COMMAND_TOPIC,
    ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN,
    MQTTClimateComponent,
    MQTTComponent,
    MQTTSwitchComponent,
    _validate_topic_lengths,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEVICES,
    CONF_ESPHOME,
    CONF_ID,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

# "dev/switch/<object_id>/command" is 19 characters around the object id
FITS = "a" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN - 19)
TOO_LONG = FITS + "a"
LONG_LITERAL = "t" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN + 1)


def _mqtt_id(cls: MockObjClass) -> ID:
    return ID("mqtt_id", is_declaration=True, type=cls)


def _switch(name: str, **extra) -> dict:
    return {
        "switch": [
            {CONF_NAME: name, CONF_MQTT_ID: _mqtt_id(MQTTSwitchComponent), **extra}
        ]
    }


def _run(
    monkeypatch: pytest.MonkeyPatch,
    full_config: dict,
    mqtt: ConfigType | None = None,
    platform: str = PLATFORM_ESP8266,
) -> ConfigType:
    monkeypatch.setitem(CORE.data, KEY_CORE, {KEY_TARGET_PLATFORM: platform})
    monkeypatch.setattr(CORE, "name", "dev")
    monkeypatch.setattr(CORE, "friendly_name", None)
    config = {"topic_prefix": "dev", **(mqtt or {})}
    token = fv.full_config.set({CONF_ESPHOME: {}, **full_config})
    try:
        return _validate_topic_lengths(config)
    finally:
        fv.full_config.reset(token)


@pytest.mark.parametrize(
    ("full_config", "mqtt", "platform"),
    [
        pytest.param(_switch(TOO_LONG), None, PLATFORM_ESP8266, id="command_topic"),
        pytest.param(_switch(TOO_LONG), None, PLATFORM_BK72XX, id="libretiny"),
        pytest.param(
            {CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True}, **_switch(FITS)},
            None,
            PLATFORM_ESP8266,
            id="mac_suffix",
        ),
        pytest.param(
            _switch("s", command_topic=LONG_LITERAL),
            None,
            PLATFORM_ESP8266,
            id="custom_command_topic",
        ),
        pytest.param(
            {
                "climate": [
                    # "dev/climate/<object_id>/mode/command" is 25 characters around it
                    {CONF_NAME: "a" * 104, CONF_MQTT_ID: _mqtt_id(MQTTClimateComponent)}
                ]
            },
            None,
            PLATFORM_ESP8266,
            id="climate_mode_command",
        ),
        pytest.param(
            {},
            {"on_message": [{"topic": LONG_LITERAL}]},
            PLATFORM_ESP8266,
            id="trigger",
        ),
        pytest.param(
            {"sensor": [{"platform": "mqtt_subscribe", "topic": LONG_LITERAL}]},
            None,
            PLATFORM_ESP8266,
            id="mqtt_subscribe",
        ),
        pytest.param(
            {
                CONF_ESPHOME: {
                    CONF_DEVICES: [{CONF_ID: ID("sub"), CONF_NAME: TOO_LONG}]
                },
                **_switch("", device_id=ID("sub")),
            },
            None,
            PLATFORM_ESP8266,
            id="unnamed_on_sub_device",
        ),
        pytest.param(
            _switch("s", command_topic=LONG_LITERAL),
            {"topic_prefix": ""},
            PLATFORM_ESP8266,
            id="custom_topic_without_prefix",
        ),
    ],
)
def test_rejected(
    monkeypatch: pytest.MonkeyPatch, full_config: dict, mqtt: dict | None, platform: str
) -> None:
    with pytest.raises(cv.Invalid, match="characters"):
        _run(monkeypatch, full_config, mqtt, platform)


@pytest.mark.parametrize(
    ("full_config", "mqtt", "platform"),
    [
        pytest.param(_switch(FITS), None, PLATFORM_ESP8266, id="at_the_limit"),
        pytest.param(_switch(TOO_LONG), None, PLATFORM_ESP32, id="esp32"),
        pytest.param(
            _switch(TOO_LONG, command_topic="short/cmd"),
            None,
            PLATFORM_ESP8266,
            id="short_custom_topic",
        ),
        pytest.param(
            _switch(TOO_LONG, command_topic=Lambda('return "x";')),
            None,
            PLATFORM_ESP8266,
            id="lambda_topic",
        ),
        pytest.param(
            _switch(TOO_LONG, internal=True), None, PLATFORM_ESP8266, id="internal"
        ),
        pytest.param(
            {"switch": [{CONF_NAME: TOO_LONG}]}, None, PLATFORM_ESP8266, id="no_mqtt"
        ),
        pytest.param(
            _switch("s", state_topic="", command_topic=LONG_LITERAL),
            None,
            PLATFORM_ESP8266,
            id="internal_by_empty_state_topic",
        ),
        pytest.param(
            {},
            {"on_message": [{"topic": LONG_LITERAL + "/#"}]},
            PLATFORM_ESP8266,
            id="wildcard",
        ),
    ],
)
def test_accepted(
    monkeypatch: pytest.MonkeyPatch, full_config: dict, mqtt: dict | None, platform: str
) -> None:
    _run(monkeypatch, full_config, mqtt, platform)


def test_message_names_the_topic_and_length(monkeypatch: pytest.MonkeyPatch) -> None:
    with pytest.raises(cv.Invalid, match=rf"'dev/switch/{TOO_LONG}/command' is 129"):
        _run(monkeypatch, _switch(TOO_LONG))


def test_every_mqtt_entity_class_is_classified() -> None:
    """A new MQTT entity class must say whether it subscribes to a command topic."""
    declared = {
        str(value)
        for value in vars(mqtt).values()
        if isinstance(value, MockObjClass)
        and value is not MQTTComponent
        and value.inherits_from(MQTTComponent)
    }
    without = {str(cls) for cls in _MQTT_WITHOUT_COMMAND_TOPIC}
    assert not set(_MQTT_COMMAND_TOPICS) & without
    assert declared == set(_MQTT_COMMAND_TOPICS) | without
