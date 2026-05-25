"""Validation tests for the mqtt ws/wss transport options."""

from __future__ import annotations

import pytest

from esphome.components.mqtt import (
    CONF_TRANSPORT,
    CONF_WS_PATH,
    MQTT_TRANSPORT_TCP,
    MQTT_TRANSPORT_WS,
    MQTT_TRANSPORT_WSS,
    _validate_transport,
    validate_config,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_CERTIFICATE_AUTHORITY,
    CONF_TOPIC_PREFIX,
    PlatformFramework,
)

from ..types import SetCoreConfigCallable


def test_transport_ws_rejected_on_non_esp32(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test that ws/wss is rejected on non-ESP32 platforms."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    with pytest.raises(cv.Invalid, match="only supported on ESP32"):
        _validate_transport(MQTT_TRANSPORT_WS)


def test_transport_wss_rejected_on_esp32_arduino(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test that ws/wss is rejected under the Arduino framework (needs esp-idf)."""
    set_core_config(PlatformFramework.ESP32_ARDUINO)
    with pytest.raises(cv.Invalid, match="esp-idf"):
        _validate_transport(MQTT_TRANSPORT_WSS)


def test_transport_wss_accepted_on_esp32_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test that wss validates on ESP32 + esp-idf."""
    set_core_config(PlatformFramework.ESP32_IDF)
    assert _validate_transport(MQTT_TRANSPORT_WSS) == MQTT_TRANSPORT_WSS


def test_transport_tcp_accepted_off_esp32(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test that the default tcp transport stays valid on non-ESP32 targets."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    assert _validate_transport(MQTT_TRANSPORT_TCP) == MQTT_TRANSPORT_TCP


def test_ws_path_rejected_without_websocket_transport() -> None:
    """Test that ws_path is rejected when transport is not ws/wss."""
    with pytest.raises(cv.Invalid, match="only valid when"):
        validate_config(
            {
                CONF_TRANSPORT: MQTT_TRANSPORT_TCP,
                CONF_WS_PATH: "/mqtt",
                CONF_TOPIC_PREFIX: "test",
            }
        )


def test_plaintext_ws_with_ca_rejected() -> None:
    """Test that pairing a CA with unencrypted ws is rejected."""
    with pytest.raises(cv.Invalid, match="unencrypted"):
        validate_config(
            {
                CONF_TRANSPORT: MQTT_TRANSPORT_WS,
                CONF_CERTIFICATE_AUTHORITY: "-----BEGIN CERTIFICATE-----",
                CONF_TOPIC_PREFIX: "test",
            }
        )
