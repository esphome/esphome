"""Tests for choosing where an mqtt trigger topic is stored."""

import pytest

from esphome.components.mqtt import MQTT_DEFAULT_TOPIC_MAX_LEN, _topic_source
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_ESP8266,
)
from esphome.core import CORE

# "é" is two bytes in UTF-8, so these topics are one character shorter than their byte length
LIMIT = MQTT_DEFAULT_TOPIC_MAX_LEN


@pytest.mark.parametrize(
    ("topic", "platform", "source"),
    [
        pytest.param("t" * (LIMIT - 1), PLATFORM_ESP8266, "progmem_topic", id="fits"),
        pytest.param("t" * LIMIT, PLATFORM_ESP8266, "static_topic", id="too_long"),
        pytest.param(
            "é" + "t" * (LIMIT - 3), PLATFORM_ESP8266, "progmem_topic", id="utf8_fits"
        ),
        pytest.param(
            "é" + "t" * (LIMIT - 2),
            PLATFORM_ESP8266,
            "static_topic",
            id="utf8_bytes_too_long",
        ),
        pytest.param("t" * (LIMIT - 1), PLATFORM_ESP32, "static_topic", id="esp32"),
        pytest.param("é" * 10, PLATFORM_ESP32, "static_topic", id="esp32_utf8"),
    ],
)
def test_topic_source(
    monkeypatch: pytest.MonkeyPatch, topic: str, platform: str, source: str
) -> None:
    """ESP8266 keeps a topic in flash only when its UTF-8 bytes fit the copy buffer."""
    monkeypatch.setitem(CORE.data, KEY_CORE, {KEY_TARGET_PLATFORM: platform})
    assert str(_topic_source(topic)).startswith(f"mqtt::{source}(")
