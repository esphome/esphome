"""Tests for the mqtt check that subscribed topics fit the AsyncMqttClient limit."""

import pytest

from esphome.components.mqtt import (
    ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN,
    OBJECT_ID_MAX_LEN,
    MQTTClimateComponent,
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

# "dev/switch/<object_id>/command" is 19 bytes around the object id
FITS = "a" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN - 19)
TOO_LONG = FITS + "a"
LONG_LITERAL = "t" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN + 1)
# Each byte of a two-byte character sanitizes to one underscore on the device
FITS_MULTIBYTE = "é" * ((ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN - 19) // 2)
TOO_LONG_MULTIBYTE = FITS_MULTIBYTE + "é"
LONG_MULTIBYTE_LITERAL = "é" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN // 2 + 1)


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
            _switch(TOO_LONG_MULTIBYTE), None, PLATFORM_ESP8266, id="multibyte_name"
        ),
        pytest.param(
            _switch("s", command_topic=LONG_MULTIBYTE_LITERAL),
            None,
            PLATFORM_ESP8266,
            id="multibyte_literal",
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
            # An empty specialized topic means the default, unlike an empty command_topic
            {
                "climate": [
                    {
                        CONF_NAME: "a" * 104,
                        CONF_MQTT_ID: _mqtt_id(MQTTClimateComponent),
                        "mode_command_topic": "",
                        "target_temperature_command_topic": "short/temp",
                    }
                ]
            },
            None,
            PLATFORM_ESP8266,
            id="climate_empty_mode_command_topic",
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
        pytest.param(
            # A platform that declares its own switches, like sprinkler valves
            {"sprinkler": [{"valves": [_switch(TOO_LONG)["switch"][0]]}]},
            None,
            PLATFORM_ESP8266,
            id="nested_entity",
        ),
        pytest.param(
            # "dev/climate/<object_id>/target_temperature/command" is 39 around it,
            # while mode/command still fits
            {
                "climate": [
                    {CONF_NAME: "a" * 90, CONF_MQTT_ID: _mqtt_id(MQTTClimateComponent)}
                ]
            },
            None,
            PLATFORM_ESP8266,
            id="climate_target_temperature",
        ),
    ],
)
def test_rejected(
    monkeypatch: pytest.MonkeyPatch, full_config: dict, mqtt: dict | None, platform: str
) -> None:
    with pytest.raises(cv.Invalid, match="bytes"):
        _run(monkeypatch, full_config, mqtt, platform)


@pytest.mark.parametrize(
    ("full_config", "mqtt", "platform"),
    [
        pytest.param(_switch(FITS), None, PLATFORM_ESP8266, id="at_the_limit"),
        pytest.param(
            _switch(FITS_MULTIBYTE), None, PLATFORM_ESP8266, id="multibyte_at_the_limit"
        ),
        pytest.param(
            _switch("s", state_topic="dev/s", command_topic=""),
            None,
            PLATFORM_ESP8266,
            id="empty_command_topic_stays_empty",
        ),
        pytest.param(
            # The firmware subscribes to the short _low and _high overrides on a two-point
            # device, so the unused default target_temperature topic must not count
            {
                "climate": [
                    {
                        CONF_NAME: "a" * 90,
                        CONF_MQTT_ID: _mqtt_id(MQTTClimateComponent),
                        "target_temperature_low_command_topic": "short/low",
                        "target_temperature_high_command_topic": "short/high",
                    }
                ]
            },
            None,
            PLATFORM_ESP8266,
            id="climate_two_point_overrides",
        ),
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
            # The MAC suffix goes on the friendly name, never on a sub-device's name
            {
                CONF_ESPHOME: {
                    CONF_NAME_ADD_MAC_SUFFIX: True,
                    CONF_DEVICES: [{CONF_ID: ID("sub"), CONF_NAME: FITS}],
                },
                **_switch("", device_id=ID("sub")),
            },
            {"topic_prefix": "abc"},
            PLATFORM_ESP8266,
            id="sub_device_has_no_mac_suffix",
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
    with pytest.raises(
        cv.Invalid, match=rf"'dev/switch/{TOO_LONG}/command' is 129 bytes"
    ):
        _run(monkeypatch, _switch(TOO_LONG))


def test_object_id_is_capped_like_the_firmware(monkeypatch: pytest.MonkeyPatch) -> None:
    capped = "a" * (OBJECT_ID_MAX_LEN - 1)
    with pytest.raises(
        cv.Invalid, match=rf"'dev/switch/{capped}/command' is 146 bytes"
    ):
        _run(monkeypatch, _switch("a" * 300))
