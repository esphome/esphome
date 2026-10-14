"""Tests for the mqtt check that subscribed topics fit the AsyncMqttClient limit."""

import pytest

from esphome.components.mqtt import (
    ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN,
    _validate_topic_lengths,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_NAME_ADD_MAC_SUFFIX,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_BK72XX,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE, Lambda
import esphome.final_validate as fv
from esphome.types import ConfigType

# "dev/switch/<object_id>/command" is 19 characters around the object id
FITS = "a" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN - 19)
TOO_LONG = FITS + "a"


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
    full_config = {CONF_ESPHOME: {}, **full_config}
    token = fv.full_config.set(full_config)
    try:
        return _validate_topic_lengths(config)
    finally:
        fv.full_config.reset(token)


def _switch(name: str, **extra) -> dict:
    return {CONF_NAME: name, CONF_MQTT_ID: "mqtt_switch", **extra}


def test_command_topic_at_the_limit_passes(monkeypatch: pytest.MonkeyPatch) -> None:
    _run(monkeypatch, {"switch": [_switch(FITS)]})


def test_command_topic_over_the_limit_rejected(monkeypatch: pytest.MonkeyPatch) -> None:
    with pytest.raises(cv.Invalid, match="129 characters"):
        _run(monkeypatch, {"switch": [_switch(TOO_LONG)]})


def test_libretiny_is_checked(monkeypatch: pytest.MonkeyPatch) -> None:
    with pytest.raises(cv.Invalid):
        _run(monkeypatch, {"switch": [_switch(TOO_LONG)]}, platform=PLATFORM_BK72XX)


def test_esp32_is_not_checked(monkeypatch: pytest.MonkeyPatch) -> None:
    _run(monkeypatch, {"switch": [_switch(TOO_LONG)]}, platform=PLATFORM_ESP32)


def test_mac_suffix_counts(monkeypatch: pytest.MonkeyPatch) -> None:
    """The default topic_prefix gains a MAC suffix at runtime."""
    with pytest.raises(cv.Invalid):
        _run(
            monkeypatch,
            {
                CONF_ESPHOME: {CONF_NAME_ADD_MAC_SUFFIX: True},
                "switch": [_switch(FITS)],
            },
        )


def test_custom_command_topic_replaces_the_default(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    _run(monkeypatch, {"switch": [_switch(TOO_LONG, command_topic="short/cmd")]})
    with pytest.raises(cv.Invalid):
        _run(monkeypatch, {"switch": [_switch("s", command_topic="x" * 129)]})


def test_lambda_command_topic_is_skipped(monkeypatch: pytest.MonkeyPatch) -> None:
    lam = Lambda('return "x";')
    _run(monkeypatch, {"switch": [_switch(TOO_LONG, command_topic=lam)]})


def test_internal_entity_is_skipped(monkeypatch: pytest.MonkeyPatch) -> None:
    _run(monkeypatch, {"switch": [_switch(TOO_LONG, internal=True)]})


def test_entity_without_mqtt_is_skipped(monkeypatch: pytest.MonkeyPatch) -> None:
    _run(monkeypatch, {"switch": [{CONF_NAME: TOO_LONG}]})


def test_climate_uses_mode_command(monkeypatch: pytest.MonkeyPatch) -> None:
    # "dev/climate/<object_id>/mode/command" is 25 characters around the object id
    name = "a" * (ASYNC_MQTT_CLIENT_MAX_TOPIC_LEN - 24)
    climate = {CONF_NAME: name, CONF_MQTT_ID: "mqtt_climate"}
    with pytest.raises(cv.Invalid):
        _run(monkeypatch, {"climate": [climate]})


def test_trigger_and_mqtt_subscribe_literals(monkeypatch: pytest.MonkeyPatch) -> None:
    with pytest.raises(cv.Invalid):
        _run(monkeypatch, {}, mqtt={"on_message": [{"topic": "t" * 129}]})
    with pytest.raises(cv.Invalid):
        _run(
            monkeypatch,
            {"sensor": [{"platform": "mqtt_subscribe", "topic": "t" * 129}]},
        )


def test_wildcards_are_skipped(monkeypatch: pytest.MonkeyPatch) -> None:
    _run(monkeypatch, {}, mqtt={"on_message": [{"topic": "t" * 129 + "/#"}]})
