"""Tests for the ESP8266 MQTT backend."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE

ESP8266_ASYNC_MQTT_CLIENT_LIBRARY = "esphome/AsyncMqttClient-esphome"
ESP8266_ASYNC_MQTT_CLIENT_VERSION = "2.2.0"
ESP8266_ESP_ASYNC_TCP_LIBRARY = "esphome/ESPAsyncTCP"
ESP8266_ESP_ASYNC_TCP_VERSION = "2.1.0"


def test_mqtt_esp8266_codegen_uses_esp8266_mqtt_libraries(
    generate_main: Callable[[str | Path], str],
) -> None:
    """Test ESP8266 MQTT uses the ESP8266 MQTT libraries."""
    generate_main("tests/component_tests/mqtt/test_mqtt_esp8266.yaml")

    async_mqtt_client = CORE.platformio_libraries["AsyncMqttClient-esphome"]
    assert async_mqtt_client.name == ESP8266_ASYNC_MQTT_CLIENT_LIBRARY
    assert async_mqtt_client.version == ESP8266_ASYNC_MQTT_CLIENT_VERSION
    assert async_mqtt_client.repository is None

    async_tcp = CORE.platformio_libraries["ESPAsyncTCP"]
    assert async_tcp.name == ESP8266_ESP_ASYNC_TCP_LIBRARY
    assert async_tcp.version == ESP8266_ESP_ASYNC_TCP_VERSION
    assert async_tcp.repository is None
